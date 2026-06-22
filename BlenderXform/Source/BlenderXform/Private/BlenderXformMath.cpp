#include "BlenderXformMath.h"

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
                                     const FXConstraint& C, bool bNumeric, double NumericValue)
{
	if (bNumeric)
	{
		// Numeric move needs a single axis to give a direction (e.g. G X 5). Free/plane numeric is a no-op.
		if (C.Axis != EXAxis::Free && !C.bPlane)
		{
			return NumericValue * C.AxisDir().GetSafeNormal();
		}
		return FVector::ZeroVector;
	}

	return ConstrainVector(WorldNow - WorldStart, C);
}

FVector FBlenderXformMath::ScaleFactors(const FVector2D& PivotS, const FVector2D& StartS, const FVector2D& NowS,
                                        const FXConstraint& C, bool bNumeric, double NumericValue)
{
	double F;
	if (bNumeric)
	{
		F = NumericValue;
	}
	else
	{
		const double D0 = FVector2D::Distance(StartS, PivotS);
		const double D1 = FVector2D::Distance(NowS, PivotS);
		F = (D0 > KINDA_SMALL_NUMBER) ? (D1 / D0) : 1.0;
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
                                         bool bNumeric, double NumericValue)
{
	if (bNumeric)
	{
		return NumericValue;
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

	return Delta;
}
