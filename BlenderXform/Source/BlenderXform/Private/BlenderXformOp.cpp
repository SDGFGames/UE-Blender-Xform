#include "BlenderXformOp.h"
#include "BlenderXformMath.h"

void FBlenderXformOp::Begin(EXMode InMode, IXApplySink& InSink)
{
	const bool bSwitching = IsActive() && Sink == &InSink;

	Mode = InMode;
	Con = FXConstraint();
	NumBuf.Empty();
	bHasNumeric = false;
	NumVal = 0.0;

	if (!bSwitching)
	{
		// Fresh op: snapshot the selection and open the transaction. A mode switch (G->S->R) keeps
		// the original snapshot so the new op transforms from the same start and Cancel still restores it.
		Sink = &InSink;
		LastWorldStart = LastWorldNow = FVector::ZeroVector;
		LastPivotS = LastStartS = LastNowS = FVector2D::ZeroVector;
		LastCamFwd = FVector(0, 0, -1);
		Sink->Begin();
	}

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
		NumBuf.AppendChar(Ch);
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
}

void FBlenderXformOp::UpdateFromScreen(const FVector& WorldStart, const FVector& WorldNow,
                                       const FVector2D& PivotS, const FVector2D& StartS, const FVector2D& NowS,
                                       const FVector& CamForward)
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
	Recompute();
}

void FBlenderXformOp::Commit()
{
	if (!IsActive())
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

	if (Tuning.bSnap)      { S += TEXT(" [snap]"); }
	if (Tuning.bPrecision) { S += TEXT(" [fine]"); }

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
	NumVal = 0.0;
	Sink = nullptr;
	Applied = FXApplied();
	// Tuning intentionally persists across ops; it is owned/refreshed by the input processor.
}

void FBlenderXformOp::ParseNumeric()
{
	bHasNumeric = false;
	for (const TCHAR C : NumBuf)
	{
		if (C >= TEXT('0') && C <= TEXT('9'))
		{
			bHasNumeric = true;
			break;
		}
	}
	NumVal = bHasNumeric ? FCString::Atod(*NumBuf) : 0.0;
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
	A.Pivot = Sink->Pivot();

	switch (Mode)
	{
		case EXMode::Move:
			A.MoveDelta = FBlenderXformMath::MoveDelta(LastWorldStart, LastWorldNow, Con, bHasNumeric, NumVal, Tuning);
			break;
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
			A.RotDeg = FBlenderXformMath::RotateAngleDeg(LastPivotS, LastStartS, LastNowS, A.RotAxis, LastCamFwd, bHasNumeric, NumVal, Tuning);
			break;
		}
		default:
			break;
	}

	Applied = A;
	Sink->Apply(A);
}
