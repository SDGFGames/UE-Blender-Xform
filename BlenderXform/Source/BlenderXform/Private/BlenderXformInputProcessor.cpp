#include "BlenderXformInputProcessor.h"
#include "BlenderXformSettings.h"
#include "BlenderXformMath.h" // FBlenderXformMath::RayPlaneIntersect

#include "CoreGlobals.h" // GFrameCounter
#include "Editor.h"      // GEditor (viewport grid snap sizes)
#include "Settings/LevelEditorViewportSettings.h" // per-mode grid-snap toggles
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h" // FModifierKeysState
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "LevelEditorViewport.h"
#include "SceneView.h"
#include "UnrealClient.h"

#if PLATFORM_MAC
// Read the hardware Escape key directly; macOS can consume it before any input pre-processor.
THIRD_PARTY_INCLUDES_START
#include <CoreGraphics/CGEventSource.h>
THIRD_PARTY_INCLUDES_END
#endif

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

}

bool FBlenderXformInputPolicy::ShouldConsumeUnsafeEditorShortcut(const FKey& Key, bool bControlDown, bool bCommandDown)
{
	if (!bControlDown && !bCommandDown)
	{
		return false;
	}

	return Key == EKeys::Z || Key == EKeys::Y || Key == EKeys::S;
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

	// Don't hijack G/S/R while the user is navigating the viewport: WASD flight needs S (and the rest)
	// while RMB is held, orbit uses Alt+LMB, pan uses MMB. If any mouse button is down, let the key
	// fall through to UE — Blender likewise never starts a transform during navigation.
	if (VC->Viewport->KeyState(EKeys::RightMouseButton) ||
	    VC->Viewport->KeyState(EKeys::MiddleMouseButton) ||
	    VC->Viewport->KeyState(EKeys::LeftMouseButton))
	{
		return false;
	}

	return Sink.HasSelection();
}

void FBlenderXformInputProcessor::CaptureStartCursor()
{
	// Read the settings-derived feel knobs once per op; per-move we only patch in Ctrl/Shift.
	CachedBaseTuning = BuildTuning(false, false);
	StartPixel = ViewportMousePixel(ActiveLevelViewport());
	UpdateFromMouse();
}

FXTuning FBlenderXformInputProcessor::BuildTuning(bool bSnap, bool bPrecision) const
{
	FXTuning T;
	if (const UBlenderXformSettings* S = GetDefault<UBlenderXformSettings>())
	{
		T.MouseSensitivity  = S->MouseSensitivity;
		T.RotateSensitivity = S->RotateSensitivity;
		T.PrecisionScale    = S->PrecisionScale;
	}
	// Snap step sizes are bound to UE's viewport grid (translate/rotate/scale), so there's a single
	// place to set them. Move/rotate snap their delta to UE's grid like the native gizmo; scale snaps
	// the multiplicative factor to UE's scale step (Blender-style) — which equals the native gizmo's
	// absolute snap only when the start scale is 1. FXTuning's defaults stand in if GEditor is null.
	if (GEditor)
	{
		T.MoveSnap      = GEditor->GetGridSize();
		T.RotateSnapDeg = GEditor->GetRotGridSize().Yaw;
		T.ScaleSnap     = GEditor->GetScaleGridSize();
	}
	T.bSnap = bSnap;
	T.bPrecision = bPrecision;
	return T;
}

void FBlenderXformInputProcessor::RefreshTuningFromModifiers()
{
	// We only READ modifier state here; we never consume Ctrl/Shift key events, so Cmd/Ctrl+Z,
	// Shift+axis (plane), etc. are all untouched.
	const FModifierKeysState M = FSlateApplication::Get().GetModifierKeys();
	bLastShiftDown = M.IsShiftDown();
	bLastSnapActive = ComputeSnapActive();

	FXTuning T = CachedBaseTuning; // sizes/sensitivity read once at op start; patch live state here
	T.bSnap = bLastSnapActive;
	T.bPrecision = bLastShiftDown;
	Op.SetTuning(T);
}

bool FBlenderXformInputProcessor::ComputeSnapActive() const
{
	// Snap engages when UE's grid snap for the CURRENT op's mode is on (so we follow the native gizmo's
	// snap state), or when Ctrl is held (Blender on-demand snap). Step sizes come from the same UE grid
	// (see BuildTuning).
	if (const ULevelEditorViewportSettings* VP = GetDefault<ULevelEditorViewportSettings>())
	{
		switch (Op.GetMode())
		{
			case EXMode::Move:   if (VP->GridEnabled)      { return true; } break;
			case EXMode::Rotate: if (VP->RotGridEnabled)   { return true; } break;
			case EXMode::Scale:  if (VP->SnapScaleEnabled) { return true; } break;
			default: break;
		}
	}
	return FSlateApplication::Get().GetModifierKeys().IsControlDown();
}

void FBlenderXformInputProcessor::UpdateFromMouse()
{
	FLevelEditorViewportClient* VC = ActiveLevelViewport();
	if (!VC || !VC->Viewport || !Op.IsActive())
	{
		return;
	}

	RefreshTuningFromModifiers();

	if (!EnsureViewCache(VC))
	{
		return;
	}

	const FVector Pivot = Sink.Pivot();

	FVector2D PivotPixel;
	if (!FSceneView::ProjectWorldToScreen(Pivot, CachedViewRect, CachedViewProj, PivotPixel))
	{
		return; // pivot is behind the camera; deprojecting against it would yield a garbage delta
	}

	const FVector2D NowPixel = ViewportMousePixel(VC);
	FVector WorldStart = FVector::ZeroVector;
	FVector WorldNow = FVector::ZeroVector;
	if (!DeprojectToPlane(StartPixel, Pivot, CachedCamFwd, WorldStart) ||
	    !DeprojectToPlane(NowPixel, Pivot, CachedCamFwd, WorldNow))
	{
		return;
	}

	Op.UpdateFromScreen(WorldStart, WorldNow, PivotPixel, StartPixel, NowPixel, CachedCamFwd);
}

bool FBlenderXformInputProcessor::EnsureViewCache(FLevelEditorViewportClient* VC)
{
	const uint64 Frame = GFrameCounter;
	if (bViewCacheValid && CachedViewFrame == Frame && CachedViewVC == VC)
	{
		return true; // already computed this frame for this viewport
	}

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		VC->Viewport, VC->GetScene(), VC->EngineShowFlags).SetRealtimeUpdate(VC->IsRealtime()));
	FSceneView* View = VC->CalcSceneView(&ViewFamily);
	if (!View)
	{
		bViewCacheValid = false;
		return false;
	}

	CachedViewRect    = View->UnscaledViewRect;
	CachedViewProj    = View->ViewMatrices.GetViewProjectionMatrix();
	CachedInvViewProj = View->ViewMatrices.GetInvViewProjectionMatrix();
	CachedCamFwd      = View->GetViewDirection();
	CachedViewFrame   = Frame;
	CachedViewVC      = VC;
	bViewCacheValid   = true;
	return true;
}

bool FBlenderXformInputProcessor::DeprojectToPlane(const FVector2D& Pixel, const FVector& PlanePt, const FVector& PlaneN, FVector& OutWorld) const
{
	FVector Origin, Dir;
	FSceneView::DeprojectScreenToWorld(Pixel, CachedViewRect, CachedInvViewProj, Origin, Dir);
	bool bValid = false;
	OutWorld = FBlenderXformMath::RayPlaneIntersect(Origin, Dir, PlanePt, PlaneN, bValid);
	return bValid;
}

void FBlenderXformInputProcessor::Tick(const float, FSlateApplication&, TSharedRef<ICursor>)
{
	// Re-assert any post-cancel selection first — this must run even with no active op, because UE's
	// native Escape-deselect lands a frame or two after we cancel. No-op when nothing is pending.
	Sink.DrainReselect();

	if (!Op.IsActive())
	{
		return;
	}

	// Safety: never leave an op stranded (which would swallow all input). If the toggle went off, the
	// viewport vanished, or every snapshotted actor was deleted mid-op, cancel and hand control back.
	if (!IsEnabled() || !ActiveLevelViewport() || !Sink.HasLiveSnapshot())
	{
		Op.Cancel();
		return;
	}

	// Re-apply the instant the snap state (Ctrl OR the UE grid-snap toggle) or Shift changes, even with
	// a still cursor (Blender lets you tap Ctrl mid-op to snap in place; flipping UE's snap button mid-op
	// counts too). Only fires on a state change, so the steady-state per-frame cost stays nil.
	{
		const FModifierKeysState M = FSlateApplication::Get().GetModifierKeys();
		if (ComputeSnapActive() != bLastSnapActive || M.IsShiftDown() != bLastShiftDown)
		{
			UpdateFromMouse();
		}
	}

#if PLATFORM_MAC
	// Physical Escape is unreliable through Slate on macOS (it can be consumed before any input
	// pre-processor sees it), so poll the hardware key state every frame — Blender's Esc-to-cancel.
	// kVK_Escape == 53.
	if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, (CGKeyCode)53))
	{
		Op.Cancel();
	}
#endif
}

bool FBlenderXformInputProcessor::HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& InKeyEvent)
{
	const FKey K = InKeyEvent.GetKey();
	const bool bShift = InKeyEvent.IsShiftDown();

	if (Op.IsActive())
	{
		if (FBlenderXformInputPolicy::ShouldConsumeUnsafeEditorShortcut(K, InKeyEvent.IsControlDown(), InKeyEvent.IsCommandDown()))
		{
			return true;
		}

		// Let most Cmd/Ctrl/Alt chords through so editor shortcuts (including our Alt+Shift+B toggle) keep
		// working during a modal op. Undo/redo/save are consumed above because they would mutate editor
		// state while our transaction is still open.
		if (InKeyEvent.IsCommandDown() || InKeyEvent.IsControlDown() || InKeyEvent.IsAltDown())
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

	// Idle Shift+D duplicates the selection and immediately grabs it (Blender). Like G/S/R it only fires
	// over the viewport with a selection; otherwise it falls through (UE has no native Shift+D binding).
	if (K == EKeys::D && bShift &&
	    !InKeyEvent.IsControlDown() && !InKeyEvent.IsCommandDown() && !InKeyEvent.IsAltDown())
	{
		if (!CanStartHere())
		{
			return false;
		}
		// edactDuplicateSelected duplicates COMPONENTS (not actors) when a component is sub-selected
		// (in-viewport component edit mode); that's not what Shift+D means here, so fall through cleanly
		// rather than start an op that would just churn a no-op transaction. G/S/R are unaffected.
		if (GEditor && GEditor->GetSelectedComponentCount() > 0)
		{
			return false;
		}
		Op.BeginDuplicate(Sink);
		CaptureStartCursor();
		return true;
	}

	// Idle: G/S/R start an op, and only over the viewport with a selection. Everything else
	// (including G/R when those conditions aren't met) passes through to native UE.
	if (K == EKeys::G || K == EKeys::S || K == EKeys::R)
	{
		if (!CanStartHere())
		{
			return false;
		}
		const EXMode Mode = (K == EKeys::G) ? EXMode::Move : (K == EKeys::S ? EXMode::Scale : EXMode::Rotate);

		// Alt+G/S/R clears that component instantly (Blender: Alt+G location, Alt+S scale, Alt+R
		// rotation), one undo step. No modal op — just reset and hand control back.
		if (InKeyEvent.IsAltDown())
		{
			Sink.ClearComponent(Mode);
			return true;
		}

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
