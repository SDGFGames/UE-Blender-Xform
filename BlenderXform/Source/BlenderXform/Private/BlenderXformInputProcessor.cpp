#include "BlenderXformInputProcessor.h"
#include "BlenderXformSettings.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "LevelEditorViewport.h"
#include "SceneView.h"
#include "UnrealClient.h"

namespace
{
	/** Map a key to the digit/sign/dot char it represents for numeric entry, or 0 if it isn't one. */
	TCHAR NumericCharFor(const FKey& K)
	{
		if (K == EKeys::Zero  || K == EKeys::NumPadZero)  return TEXT('0');
		if (K == EKeys::One   || K == EKeys::NumPadOne)   return TEXT('1');
		if (K == EKeys::Two   || K == EKeys::NumPadTwo)   return TEXT('2');
		if (K == EKeys::Three || K == EKeys::NumPadThree) return TEXT('3');
		if (K == EKeys::Four  || K == EKeys::NumPadFour)  return TEXT('4');
		if (K == EKeys::Five  || K == EKeys::NumPadFive)  return TEXT('5');
		if (K == EKeys::Six   || K == EKeys::NumPadSix)   return TEXT('6');
		if (K == EKeys::Seven || K == EKeys::NumPadSeven) return TEXT('7');
		if (K == EKeys::Eight || K == EKeys::NumPadEight) return TEXT('8');
		if (K == EKeys::Nine  || K == EKeys::NumPadNine)  return TEXT('9');
		if (K == EKeys::Period || K == EKeys::Decimal)    return TEXT('.');
		if (K == EKeys::Hyphen || K == EKeys::Subtract)   return TEXT('-');
		return 0;
	}

	FLevelEditorViewportClient* ActiveLevelViewport()
	{
		return GCurrentLevelEditingViewportClient;
	}

	/** Cursor pixel within the active viewport, or (-1,-1) if unavailable. */
	FVector2D ViewportMousePixel(FLevelEditorViewportClient* VC)
	{
		if (VC && VC->Viewport)
		{
			FIntPoint MP;
			VC->Viewport->GetMousePos(MP);
			return FVector2D(MP.X, MP.Y);
		}
		return FVector2D(-1.0, -1.0);
	}

	/** Intersect the ray deprojected from Pixel with the plane (PlanePt, PlaneN). */
	FVector DeprojectToPlane(FSceneView* View, const FVector2D& Pixel, const FVector& PlanePt, const FVector& PlaneN)
	{
		FVector Origin, Dir;
		View->DeprojectFVector2D(Pixel, Origin, Dir);
		const double Denom = FVector::DotProduct(Dir, PlaneN);
		if (FMath::Abs(Denom) < 1e-6)
		{
			return PlanePt;
		}
		const double T = FVector::DotProduct(PlanePt - Origin, PlaneN) / Denom;
		return Origin + Dir * T;
	}
}

bool FBlenderXformInputProcessor::IsEnabled() const
{
	const UBlenderXformSettings* S = GetDefault<UBlenderXformSettings>();
	return S && S->bEnabled;
}

bool FBlenderXformInputProcessor::CanStartHere() const
{
	if (!IsEnabled())
	{
		return false;
	}
	FLevelEditorViewportClient* VC = ActiveLevelViewport();
	if (!VC || !VC->Viewport)
	{
		return false;
	}
	// Require the cursor to be inside THIS viewport, so G/S/R only fire over the 3D view.
	const FVector2D Px = ViewportMousePixel(VC);
	const FIntPoint Size = VC->Viewport->GetSizeXY();
	if (Px.X < 0.0 || Px.Y < 0.0 || Px.X >= Size.X || Px.Y >= Size.Y)
	{
		return false;
	}
	return Sink.HasSelection();
}

void FBlenderXformInputProcessor::CaptureStartCursor()
{
	StartPixel = ViewportMousePixel(ActiveLevelViewport());
	UpdateFromMouse();
}

void FBlenderXformInputProcessor::UpdateFromMouse()
{
	FLevelEditorViewportClient* VC = ActiveLevelViewport();
	if (!VC || !VC->Viewport || !Op.IsActive())
	{
		return;
	}

	const FVector2D NowPixel = ViewportMousePixel(VC);

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		VC->Viewport, VC->GetScene(), VC->EngineShowFlags).SetRealtimeUpdate(VC->IsRealtime()));
	FSceneView* View = VC->CalcSceneView(&ViewFamily);
	if (!View)
	{
		return;
	}

	const FVector Pivot = Sink.Pivot();
	const FVector CamFwd = View->GetViewDirection();

	FVector2D PivotPixel;
	FSceneView::ProjectWorldToScreen(Pivot, View->UnscaledViewRect, View->ViewMatrices.GetViewProjectionMatrix(), PivotPixel);

	const FVector WorldStart = DeprojectToPlane(View, StartPixel, Pivot, CamFwd);
	const FVector WorldNow   = DeprojectToPlane(View, NowPixel,   Pivot, CamFwd);

	Op.UpdateFromScreen(WorldStart, WorldNow, PivotPixel, StartPixel, NowPixel, CamFwd);
}

void FBlenderXformInputProcessor::Tick(const float, FSlateApplication&, TSharedRef<ICursor>)
{
	// Safety: never leave an op stranded (which would swallow all input). If the toggle went off or
	// the viewport vanished mid-op, cancel and hand control back to UE.
	if (Op.IsActive() && (!IsEnabled() || !ActiveLevelViewport()))
	{
		Op.Cancel();
	}
}

bool FBlenderXformInputProcessor::HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& InKeyEvent)
{
	const FKey K = InKeyEvent.GetKey();
	const bool bShift = InKeyEvent.IsShiftDown();

	if (Op.IsActive())
	{
		// Always let Cmd/Ctrl chords through, so system/editor shortcuts (Cmd+Z undo, Cmd+S save, ...)
		// are never swallowed by an active op. Core to not breaking native UE.
		if (InKeyEvent.IsCommandDown() || InKeyEvent.IsControlDown())
		{
			return false;
		}

		if (K == EKeys::X) { Op.CycleAxis(EXAxis::X, bShift); UpdateFromMouse(); return true; }
		if (K == EKeys::Y) { Op.CycleAxis(EXAxis::Y, bShift); UpdateFromMouse(); return true; }
		if (K == EKeys::Z) { Op.CycleAxis(EXAxis::Z, bShift); UpdateFromMouse(); return true; }

		if (K == EKeys::G) { Op.Begin(EXMode::Move,   Sink); CaptureStartCursor(); return true; }
		if (K == EKeys::S) { Op.Begin(EXMode::Scale,  Sink); CaptureStartCursor(); return true; }
		if (K == EKeys::R) { Op.Begin(EXMode::Rotate, Sink); CaptureStartCursor(); return true; }

		if (const TCHAR Ch = NumericCharFor(K)) { Op.PushDigit(Ch); return true; }
		if (K == EKeys::BackSpace) { Op.Backspace(); return true; }

		if (K == EKeys::Enter || K == EKeys::SpaceBar) { Op.Commit(); return true; }
		if (K == EKeys::Escape) { Op.Cancel(); return true; }

		// Modal: swallow every other key so editor shortcuts don't fire mid-transform.
		return true;
	}

	// Idle: only G/S/R start an op, and only over the viewport with a selection. Everything else
	// (including G/R when those conditions aren't met) passes through to native UE.
	if (K == EKeys::G || K == EKeys::S || K == EKeys::R)
	{
		if (!CanStartHere())
		{
			return false;
		}
		const EXMode Mode = (K == EKeys::G) ? EXMode::Move : (K == EKeys::S ? EXMode::Scale : EXMode::Rotate);
		Op.Begin(Mode, Sink);
		CaptureStartCursor();
		return true;
	}

	return false;
}

bool FBlenderXformInputProcessor::HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& InKeyEvent)
{
	// Belt-and-suspenders for Escape: on macOS the Escape key-DOWN can be swallowed before it
	// reaches input pre-processors, so also honor it on key-up. RMB already cancels too.
	if (Op.IsActive() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		Op.Cancel();
		return true;
	}
	return false;
}

bool FBlenderXformInputProcessor::HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent&)
{
	if (Op.IsActive())
	{
		UpdateFromMouse();
		// Don't consume: let the viewport keep its own cursor bookkeeping. The transform is already applied.
	}
	return false;
}

bool FBlenderXformInputProcessor::HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& MouseEvent)
{
	if (!Op.IsActive())
	{
		return false;
	}
	const FKey Button = MouseEvent.GetEffectingButton();
	if (Button == EKeys::LeftMouseButton)  { Op.Commit(); return true; }
	if (Button == EKeys::RightMouseButton) { Op.Cancel(); return true; }
	return true; // swallow other buttons during the op
}
