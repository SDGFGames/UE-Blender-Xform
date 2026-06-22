#include "BlenderXformMath.h"

namespace
{
	/** Round to the nearest multiple of Step (no-op for non-positive Step). */
	double SnapTo(double Value, double Step)
	{
		return (Step > KINDA_SMALL_NUMBER) ? FMath::GridSnap(Value, Step) : Value;
	}
}

FVector FBlenderXformMath::ConstrainVector(const FVector& V, const FXConstraint& C)
{
	if (C.Axis == EXAxis::Free)
	{
		return V;
	}

	const FVector Dir = C.AxisDir().GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return V;
	}

	const FVector Along = FVector::DotProduct(V, Dir) * Dir;
	return C.bPlane ? (V - Along) : Along;
}

FVector FBlenderXformMath::MoveDelta(const FVector& WorldStart, const FVector& WorldNow,
                                     const FXConstraint& C, bool bNumeric, double NumericValue,
                                     const FXTuning& T)
{
	if (bNumeric)
	{
		// Numeric move needs a single axis to give a direction (e.g. G X 5). Free/plane numeric is a no-op.
		// Exact: feel knobs never touch typed values.
		if (C.Axis != EXAxis::Free && !C.bPlane)
		{
			return NumericValue * C.AxisDir().GetSafeNormal();
		}
		return FVector::ZeroVector;
	}

	const FVector Raw = (WorldNow - WorldStart) * T.MoveGain();

	if (!T.bSnap)
	{
		return ConstrainVector(Raw, C);
	}

	// Snap along the constraint so Local snapping follows the rotated basis (not world axes).
	if (C.Axis == EXAxis::Free)
	{
		return FVector(SnapTo(Raw.X, T.MoveSnap), SnapTo(Raw.Y, T.MoveSnap), SnapTo(Raw.Z, T.MoveSnap));
	}

	const FVector BX = C.BX.GetSafeNormal();
	const FVector BY = C.BY.GetSafeNormal();
	const FVector BZ = C.BZ.GetSafeNormal();

	if (!C.bPlane)
	{
		const FVector Dir = C.AxisDir().GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			return ConstrainVector(Raw, C);
		}
		return SnapTo(FVector::DotProduct(Raw, Dir), T.MoveSnap) * Dir;
	}

	// Plane: keep the two free basis axes, snap each, drop the locked one.
	const double CX = (C.Axis == EXAxis::X) ? 0.0 : SnapTo(FVector::DotProduct(Raw, BX), T.MoveSnap);
	const double CY = (C.Axis == EXAxis::Y) ? 0.0 : SnapTo(FVector::DotProduct(Raw, BY), T.MoveSnap);
	const double CZ = (C.Axis == EXAxis::Z) ? 0.0 : SnapTo(FVector::DotProduct(Raw, BZ), T.MoveSnap);
	return CX * BX + CY * BY + CZ * BZ;
}

FVector FBlenderXformMath::ScaleFactors(const FVector2D& PivotS, const FVector2D& StartS, const FVector2D& NowS,
                                        const FXConstraint& C, bool bNumeric, double NumericValue,
                                        const FXTuning& T)
{
	double F;
	if (bNumeric)
	{
		F = NumericValue; // exact
	}
	else
	{
		const double D0 = FVector2D::Distance(StartS, PivotS);
		const double D1 = FVector2D::Distance(NowS, PivotS);
		double Raw = (D0 > KINDA_SMALL_NUMBER) ? (D1 / D0) : 1.0;
		// Sensitivity/precision scale how fast the factor departs from 1; snap quantizes it.
		Raw = 1.0 + (Raw - 1.0) * T.MoveGain();
		F = T.bSnap ? SnapTo(Raw, T.ScaleSnap) : Raw;
		// A mouse drag must never collapse to zero or mirror-flip — Blender only flips on a typed
		// negative (the bNumeric path above). Floor at one snap step when snapping (keeps a clean
		// increment), else a tiny epsilon (lets the object scale arbitrarily small but stay valid).
		F = FMath::Max(F, T.bSnap ? T.ScaleSnap : KINDA_SMALL_NUMBER);
	}

	FVector Out(1.0, 1.0, 1.0);

	if (C.Axis == EXAxis::Free)
	{
		Out = FVector(F, F, F);
	}
	else if (!C.bPlane)
	{
		// single axis: only that component scales
		if (C.Axis == EXAxis::X) { Out.X = F; }
		else if (C.Axis == EXAxis::Y) { Out.Y = F; }
		else if (C.Axis == EXAxis::Z) { Out.Z = F; }
	}
	else
	{
		// plane: the two non-locked components scale, the locked one stays 1
		Out = FVector(F, F, F);
		if (C.Axis == EXAxis::X) { Out.X = 1.0; }
		else if (C.Axis == EXAxis::Y) { Out.Y = 1.0; }
		else if (C.Axis == EXAxis::Z) { Out.Z = 1.0; }
	}

	return Out;
}

double FBlenderXformMath::RotateAngleDeg(const FVector2D& PivotS, const FVector2D& StartS, const FVector2D& NowS,
                                         const FVector& AxisWorld, const FVector& CamForward,
                                         bool bNumeric, double NumericValue,
                                         const FXTuning& T)
{
	if (bNumeric)
	{
		return NumericValue; // exact
	}

	const FVector2D A = StartS - PivotS;
	const FVector2D B = NowS - PivotS;
	if (A.IsNearlyZero() || B.IsNearlyZero())
	{
		return 0.0;
	}

	const double AngA = FMath::Atan2(A.Y, A.X);
	const double AngB = FMath::Atan2(B.Y, B.X);
	double Delta = FMath::RadiansToDegrees(AngB - AngA);

	// Normalize to (-180, 180].
	while (Delta > 180.0) { Delta -= 360.0; }
	while (Delta <= -180.0) { Delta += 360.0; }

	// Screen-space CCW must map to right-hand rotation about the world axis. When the axis points
	// away from the viewer (aligned with CamForward, which points into the screen), flip the sign.
	const double Facing = FVector::DotProduct(AxisWorld.GetSafeNormal(), CamForward.GetSafeNormal());
	if (Facing > 0.0)
	{
		Delta = -Delta;
	}

	// Sensitivity/precision scale the sweep; snap quantizes to fixed increments (Blender Ctrl=5deg).
	Delta *= T.RotGain();
	if (T.bSnap)
	{
		Delta = SnapTo(Delta, T.RotateSnapDeg);
	}

	return Delta;
}
