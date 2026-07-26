#include "BlenderXformOp.h"
#include "BlenderXformMath.h"

namespace
{
	bool MatrixNearlyEqual(const FMatrix& A, const FMatrix& B, double Tolerance)
	{
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				if (!FMath::IsNearlyEqual(A.M[Row][Col], B.M[Row][Col], Tolerance))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool AppliedNearlyEqual(const FXApplied& A, const FXApplied& B)
	{
		constexpr double Tolerance = 1e-6;
		return A.Mode == B.Mode &&
			A.MoveDelta.Equals(B.MoveDelta, Tolerance) &&
			A.ScaleFac.Equals(B.ScaleFac, Tolerance) &&
			MatrixNearlyEqual(A.ScaleMat, B.ScaleMat, Tolerance) &&
			FMath::IsNearlyEqual(A.RotDeg, B.RotDeg, Tolerance) &&
			A.RotAxis.Equals(B.RotAxis, Tolerance) &&
			A.Pivot.Equals(B.Pivot, Tolerance) &&
			A.bAlignApplied == B.bAlignApplied &&
			A.AlignPivot.Equals(B.AlignPivot, Tolerance) &&
			(A.AlignQuat.Equals(B.AlignQuat, Tolerance));
	}
}

void FBlenderXformOp::Begin(EXMode InMode, IXApplySink& InSink)
{
	// Handing off to a different sink while active would overwrite Sink and leak its open transaction.
	// Cleanly cancel the old op first. (Today there's a single sink, so this is belt-and-suspenders.)
	if (IsActive() && Sink && Sink != &InSink)
	{
		Sink->Cancel();
		Reset();
	}

	const bool bSwitching = IsActive() && Sink == &InSink;

	Mode = InMode;
	Con = FXConstraint();
	NumBuf.Empty();
	bHasNumeric = false;
	bNumericInvalid = false;
	bNumericNeedsAxis = false;
	NumVal = 0.0;
	LastSurfaceSnap = FXSurfaceSnapState();
	bSurfaceSnapApplied = false;

	if (!bSwitching)
	{
		// Fresh op: snapshot the selection and open the transaction. A mode switch (G->S->R) keeps
		// the original snapshot so the new op transforms from the same start and Cancel still restores it.
		Sink = &InSink;
		bDuplicate = false; // a plain G/S/R is never a duplicate (a mode-switch keeps the flag instead)
		LastWorldStart = LastWorldNow = FVector::ZeroVector;
		LastPivotS = LastStartS = LastNowS = FVector2D::ZeroVector;
		LastCamFwd = FVector(0, 0, -1);
		// Fresh rotation accumulator; the first mouse update seeds LastScreenDeg.
		AccumRotDeg = 0.0;
		bHaveLastScreenDeg = false;
		Sink->Begin();
	}
	else
	{
		// Mode switch INTO Rotate: re-seed the accumulator so it continues from the current cursor
		// position rather than carrying a stale angle from the previous mode.
		AccumRotDeg = 0.0;
		bHaveLastScreenDeg = false;
	}

	Recompute();
}

void FBlenderXformOp::BeginDuplicate(IXApplySink& InSink)
{
	// A duplicate-grab is always a fresh op (you can't switch INTO it), so end any active op first.
	if (IsActive())
	{
		Cancel();
	}

	Mode = EXMode::Move;
	Con = FXConstraint();
	NumBuf.Empty();
	bHasNumeric = false;
	bNumericInvalid = false;
	bNumericNeedsAxis = false;
	NumVal = 0.0;
	LastSurfaceSnap = FXSurfaceSnapState();
	bSurfaceSnapApplied = false;
	bDuplicate = true;

	Sink = &InSink;
	LastWorldStart = LastWorldNow = FVector::ZeroVector;
	LastPivotS = LastStartS = LastNowS = FVector2D::ZeroVector;
	LastCamFwd = FVector(0, 0, -1);
	AccumRotDeg = 0.0;
	bHaveLastScreenDeg = false;

	// The sink duplicates the selection in place and snapshots the COPIES (vs Begin(), which snapshots the
	// current selection). From here it's an ordinary Move — axis locks, numeric entry, snap, and a later
	// S/R mode-switch all work unchanged; only the sink's Commit/Cancel differ (Cancel removes the copy).
	Sink->BeginDuplicate();
	Recompute();
}

void FBlenderXformOp::CycleAxis(EXAxis Axis, bool bShiftPlane)
{
	if (!IsActive())
	{
		return;
	}

	if (Con.Axis == Axis && Con.bPlane == bShiftPlane && Con.Axis != EXAxis::Free)
	{
		// Re-pressing the same axis cycles Global -> Local -> off.
		if (Con.Orient == EXOrient::Global)
		{
			Con.Orient = EXOrient::Local;
			SetLocalBasis();
		}
		else
		{
			Con = FXConstraint(); // back to Free, world basis
		}
	}
	else
	{
		Con.Axis = Axis;
		Con.bPlane = bShiftPlane;
		Con.Orient = EXOrient::Global;
		SetWorldBasis();
	}

	// An axis change re-frames the rotation; re-seed the accumulator from the next mouse update.
	AccumRotDeg = 0.0;
	bHaveLastScreenDeg = false;

	Recompute();
}

void FBlenderXformOp::PushDigit(TCHAR Ch)
{
	if (!IsActive())
	{
		return;
	}

	if (Ch == TEXT('-'))
	{
		if (NumBuf.StartsWith(TEXT("-")))
		{
			NumBuf.RightChopInline(1);
		}
		else
		{
			NumBuf = FString(TEXT("-")) + NumBuf;
		}
	}
	else if (Ch == TEXT('.'))
	{
		if (!NumBuf.Contains(TEXT(".")))
		{
			NumBuf.AppendChar(TEXT('.'));
		}
	}
	else if (Ch >= TEXT('0') && Ch <= TEXT('9'))
	{
		// Cap the buffer so an absurdly long entry can't overflow or parse to inf (Task 2.3). The sign
		// toggle above is excluded, so '-' still works on a full buffer.
		if (NumBuf.Len() < 16)
		{
			NumBuf.AppendChar(Ch);
		}
	}

	ParseNumeric();
	Recompute();
}

void FBlenderXformOp::Backspace()
{
	if (!IsActive() || NumBuf.IsEmpty())
	{
		return;
	}
	NumBuf.LeftChopInline(1);
	ParseNumeric();
	Recompute();
}

void FBlenderXformOp::SetTuning(const FXTuning& InTuning)
{
	// Plain store: the caller always follows with a mouse/numeric update that recomputes once, so the
	// hot per-mouse-move path stays single-apply. (The processor re-feeds the cursor when only a
	// modifier changed, so Ctrl/Shift toggles still re-apply.)
	Tuning = InTuning;

	// Refresh the HUD badges here too: tuning is pushed at the top of the cursor update, which can
	// early-return (pivot behind camera, view not ready) before reaching Recompute — without this the
	// [snap]/[fine] badge could lag the actual snapping state for the rest of the op.
	if (IsActive())
	{
		CachedHud = HudString();
	}
}

void FBlenderXformOp::UpdateFromScreen(const FVector& WorldStart, const FVector& WorldNow,
                                       const FVector2D& PivotS, const FVector2D& StartS, const FVector2D& NowS,
                                       const FVector& CamForward, const FXSurfaceSnapState& SurfaceSnap)
{
	if (!IsActive())
	{
		return;
	}
	LastWorldStart = WorldStart;
	LastWorldNow = WorldNow;
	LastPivotS = PivotS;
	LastStartS = StartS;
	LastNowS = NowS;
	LastCamFwd = CamForward;
	LastSurfaceSnap = SurfaceSnap;
	Recompute();
}

void FBlenderXformOp::Commit()
{
	if (!IsActive())
	{
		return;
	}
	// Task 2.3: a malformed numeric entry or a Free/Plane Move numeric (no axis) has no well-defined
	// result - block Commit and leave the op active so the user can fix or backspace it.
	if (bNumericInvalid || bNumericNeedsAxis)
	{
		return;
	}
	if (Sink)
	{
		Sink->Commit();
	}
	Reset();
}

void FBlenderXformOp::Cancel()
{
	if (!IsActive())
	{
		return;
	}
	if (Sink)
	{
		Sink->Cancel();
	}
	Reset();
}

FString FBlenderXformOp::HudString() const
{
	if (!IsActive())
	{
		return FString();
	}

	FString S;
	switch (Mode)
	{
		case EXMode::Move:   S = TEXT("Move");   break;
		case EXMode::Scale:  S = TEXT("Scale");  break;
		case EXMode::Rotate: S = TEXT("Rotate"); break;
		default: break;
	}

	if (Con.Axis != EXAxis::Free)
	{
		const TCHAR* AxisName =
			Con.Axis == EXAxis::X ? TEXT("X") : (Con.Axis == EXAxis::Y ? TEXT("Y") : TEXT("Z"));
		S += Con.bPlane ? FString::Printf(TEXT(" | plane %s"), AxisName)
		                : FString::Printf(TEXT(" | %s"), AxisName);
		if (Con.Orient == EXOrient::Local)
		{
			S += TEXT(" (local)");
		}
	}

	if (!NumBuf.IsEmpty())
	{
		// Typed value (exact). Show the buffer as-is so a trailing '.' or '-' is visible while typing.
		S += FString::Printf(TEXT(" | %s"), *NumBuf);
		// Task 2.3: surface why a typed entry isn't applying. [invalid] for a non-finite/malformed parse;
		// [axis required] for a Free/Plane Move numeric (no direction until an axis is picked); [zero] warns
		// a typed 0 scale collapses the object (negative is allowed - Blender mirror).
		if (bNumericInvalid) { S += TEXT(" [invalid]"); }
		else if (bNumericNeedsAxis) { S += TEXT(" [axis required]"); }
		else if (Mode == EXMode::Scale && bHasNumeric && FMath::IsNearlyZero(NumVal)) { S += TEXT(" [zero]"); }
	}
	else
	{
		// Live mouse-driven value, Blender-style (distance / factor / degrees).
		switch (Mode)
		{
			case EXMode::Move:   S += FString::Printf(TEXT(" | %.2f"), Applied.MoveDelta.Size()); break;
			case EXMode::Scale:  S += FString::Printf(TEXT(" | x%.3f"), LargestScaleComponent(Applied.ScaleFac)); break;
			case EXMode::Rotate: S += FString::Printf(TEXT(" | %.1f deg"), Applied.RotDeg); break;
			default: break;
		}
	}

	if (bDuplicate)   { S += TEXT(" [copy]"); }
	if (Tuning.bSnap) { S += TEXT(" [snap]"); }
	if (Tuning.bPrecision && !bSurfaceSnapApplied) { S += TEXT(" [fine]"); }
	if (Mode == EXMode::Move && LastSurfaceSnap.bEnabled)
	{
		S += (!bHasNumeric && LastSurfaceSnap.bTraceAttempted && !bSurfaceSnapApplied)
			? TEXT(" [surface:none]") : TEXT(" [surface]");
	}

	return S;
}

FString FBlenderXformOp::CursorTag() const
{
	// Compact read-out that rides next to the cursor (Blender's mouse value). Mode is omitted (you just
	// pressed it; the bottom hint bar frames it) — only the live essentials: axis, value, active badges.
	if (!IsActive())
	{
		return FString();
	}

	FString S;

	// Axis context: a single letter for an axis lock, the two free axes for a plane, nothing when Free.
	if (Con.Axis != EXAxis::Free)
	{
		if (Con.bPlane)
		{
			if (Con.Axis != EXAxis::X) { S += TEXT("X"); }
			if (Con.Axis != EXAxis::Y) { S += TEXT("Y"); }
			if (Con.Axis != EXAxis::Z) { S += TEXT("Z"); }
		}
		else
		{
			S = Con.Axis == EXAxis::X ? TEXT("X") : (Con.Axis == EXAxis::Y ? TEXT("Y") : TEXT("Z"));
		}
	}

	auto AppendSpaced = [&S](const FString& Part)
	{
		if (!S.IsEmpty()) { S += TEXT(" "); }
		S += Part;
	};

	if (!NumBuf.IsEmpty())
	{
		// Typed value with a caret marking active entry (buffer as-is, so a trailing '.'/'-' shows).
		AppendSpaced(FString::Printf(TEXT("%s|"), *NumBuf));
		// Task 2.3: same context flags as the full HUD, kept compact for the cursor tag.
		if (bNumericInvalid) { AppendSpaced(TEXT("[invalid]")); }
		else if (bNumericNeedsAxis) { AppendSpaced(TEXT("[axis required]")); }
		else if (Mode == EXMode::Scale && bHasNumeric && FMath::IsNearlyZero(NumVal)) { AppendSpaced(TEXT("[zero]")); }
	}
	else
	{
		switch (Mode)
		{
			case EXMode::Move:   AppendSpaced(FString::Printf(TEXT("%.2f"), Applied.MoveDelta.Size())); break;
			case EXMode::Scale:  AppendSpaced(FString::Printf(TEXT("x%.3f"), LargestScaleComponent(Applied.ScaleFac))); break;
			case EXMode::Rotate: AppendSpaced(FString::Printf(TEXT("%.1f°"), Applied.RotDeg)); break;
			default: break;
		}
	}

	if (bDuplicate)   { S += TEXT(" [copy]"); }
	if (Tuning.bSnap) { S += TEXT(" [snap]"); }
	if (Tuning.bPrecision && !bSurfaceSnapApplied) { S += TEXT(" [fine]"); }
	if (Mode == EXMode::Move && LastSurfaceSnap.bEnabled)
	{
		S += (!bHasNumeric && LastSurfaceSnap.bTraceAttempted && !bSurfaceSnapApplied)
			? TEXT(" [surface:none]") : TEXT(" [surface]");
	}

	return S;
}

double FBlenderXformOp::LargestScaleComponent(const FVector& F)
{
	// The active scale factor is whichever component departs most from 1 (others are held at 1).
	const double DX = FMath::Abs(F.X - 1.0);
	const double DY = FMath::Abs(F.Y - 1.0);
	const double DZ = FMath::Abs(F.Z - 1.0);
	if (DX >= DY && DX >= DZ) { return F.X; }
	return (DY >= DZ) ? F.Y : F.Z;
}

void FBlenderXformOp::Reset()
{
	Mode = EXMode::None;
	Con = FXConstraint();
	NumBuf.Empty();
	bHasNumeric = false;
	bNumericInvalid = false;
	bNumericNeedsAxis = false;
	NumVal = 0.0;
	Sink = nullptr;
	Applied = FXApplied();
	CachedHud.Empty();
	LastSurfaceSnap = FXSurfaceSnapState();
	bSurfaceSnapApplied = false;
	bDuplicate = false;
	AccumRotDeg = 0.0;
	bHaveLastScreenDeg = false;
	// Tuning intentionally persists across ops; it is owned/refreshed by the input processor.
}

void FBlenderXformOp::ParseNumeric()
{
	bHasNumeric = false;
	bNumericInvalid = false;
	for (const TCHAR C : NumBuf)
	{
		if (C >= TEXT('0') && C <= TEXT('9'))
		{
			bHasNumeric = true;
			break;
		}
	}
	if (!bHasNumeric)
	{
		NumVal = 0.0;
		return;
	}
	// Task 2.3: reject non-finite / malformed parse (e.g. a buffer that overflows double range). An
	// invalid entry blocks Commit and shows [invalid] until corrected or backspaced.
	NumVal = FCString::Atod(*NumBuf);
	if (!FMath::IsFinite(NumVal))
	{
		bNumericInvalid = true;
		NumVal = 0.0;
	}
}

void FBlenderXformOp::SetWorldBasis()
{
	Con.BX = FVector::ForwardVector;
	Con.BY = FVector::RightVector;
	Con.BZ = FVector::UpVector;
}

void FBlenderXformOp::SetLocalBasis()
{
	if (Sink)
	{
		Sink->ActiveBasis(Con.BX, Con.BY, Con.BZ);
	}
}

void FBlenderXformOp::Recompute()
{
	if (!Sink)
	{
		return;
	}

	FXApplied A;
	A.Mode = Mode;
	bSurfaceSnapApplied = false;
	bNumericNeedsAxis = false; // recomputed each pass; set by the Move branch when relevant
	A.Pivot = Sink->Pivot();

	switch (Mode)
	{
		case EXMode::Move:
		{
			// Task 2.3: a numeric Move with no single axis (Free or plane) has no direction - flag it for
			// the [axis required] HUD and don't silently apply a zero delta. An invalid numeric parse
			// likewise applies nothing and blocks Commit.
			const bool bNumericMoveNoAxis = bHasNumeric && (Con.Axis == EXAxis::Free || Con.bPlane);
			if (bHasNumeric && bNumericInvalid)
			{
				A.MoveDelta = FVector::ZeroVector;
				break;
			}
			if (bNumericMoveNoAxis)
			{
				bNumericNeedsAxis = true;
				A.MoveDelta = FVector::ZeroVector;
				break;
			}
			const FVector Fallback = FBlenderXformMath::MoveDelta(LastWorldStart, LastWorldNow, Con, bHasNumeric, NumVal, Tuning);
			A.MoveDelta = Fallback;
			if (!bHasNumeric && LastSurfaceSnap.bEnabled && LastSurfaceSnap.Hit.bValid)
			{
				A.MoveDelta = FBlenderXformSurfaceSnap::SolveMoveDelta(
					Fallback, A.Pivot, Sink->SurfaceBounds(), Con, Tuning, LastSurfaceSnap.Hit, 0.1, bSurfaceSnapApplied);
			}
			// Surface normal-align (C hold): spin the selection so world Up follows the hit normal, about
			// the contact point. Suppressed by an axis/plane constraint (N3) and by numeric entry.
			if (!bHasNumeric && LastSurfaceSnap.bAlignHeld && Con.Axis == EXAxis::Free && LastSurfaceSnap.Hit.bValid)
			{
				A.bAlignApplied = true;
				A.AlignPivot = LastSurfaceSnap.Hit.Point;
				A.AlignQuat = FBlenderXformMath::AlignUpToNormalQuat(LastSurfaceSnap.Hit.Normal);
			}
			break;
		}
		case EXMode::Scale:
		{
			A.ScaleFac = FBlenderXformMath::ScaleFactors(LastPivotS, LastStartS, LastNowS, Con, bHasNumeric, NumVal, Tuning);
			// The active factor (the component that departs most from 1) drives a world-space scale
			// matrix, so the sink scales along the constraint's WORLD axis — Global vs Local correct
			// on rotated objects, unlike a component-wise multiply on the local Scale3D.
			const double Factor = LargestScaleComponent(A.ScaleFac);
			A.ScaleMat = FBlenderXformMath::ScaleMatrix(Con, Factor);
			break;
		}
		case EXMode::Rotate:
		{
			const FVector Axis = Con.HasConstraint() ? Con.AxisDir() : LastCamFwd;
			A.RotAxis = Axis.GetSafeNormal();
			if (bHasNumeric)
			{
				// Numeric is exact and bypasses the accumulator (Task 2.2).
				A.RotDeg = NumVal;
				AccumRotDeg = NumVal; // keep the accumulator in sync so a later mouse drag continues from here
			}
			else
			{
				// Accumulate the smallest signed frame-to-frame screen delta so a sweep across the +/-180
				// seam (179 -> -179) adds +2 instead of jumping to -179. Facing sign + gain + snap applied
				// to the per-frame delta, then snap re-quantizes the running total.
				const FVector2D DStart = LastStartS - LastPivotS;
				const FVector2D DNow = LastNowS - LastPivotS;
				if (!DNow.IsNearlyZero())
				{
					const double NowDeg = FMath::RadiansToDegrees(FMath::Atan2(DNow.Y, DNow.X));
					if (bHaveLastScreenDeg)
					{
						double FrameDelta = FBlenderXformMath::WrapDeltaDeg(LastScreenDeg, NowDeg);
						// Screen CCW maps to right-hand rotation about the axis; flip when the axis faces the
						// camera (points along CamFwd, into the screen).
						const double Facing = FVector::DotProduct(A.RotAxis, LastCamFwd.GetSafeNormal());
						if (Facing > 0.0) { FrameDelta = -FrameDelta; }
						FrameDelta *= Tuning.RotGain();
						AccumRotDeg += FrameDelta;
						if (Tuning.bSnap)
						{
							AccumRotDeg = FMath::GridSnap(AccumRotDeg, Tuning.RotateSnapDeg);
						}
					}
					LastScreenDeg = NowDeg;
					bHaveLastScreenDeg = true;
				}
				A.RotDeg = AccumRotDeg;
			}
			break;
		}
		default:
			break;
	}

	const bool bAppliedChanged = !AppliedNearlyEqual(Applied, A);
	Applied = A;
	if (bAppliedChanged)
	{
		Sink->Apply(A);
	}

	// Refresh the cached HUD string here (input-driven), so the per-frame draw just reads it.
	CachedHud = HudString();
}
