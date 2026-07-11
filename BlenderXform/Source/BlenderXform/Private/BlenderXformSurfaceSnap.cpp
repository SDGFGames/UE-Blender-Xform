#include "BlenderXformSurfaceSnap.h"
#include "BlenderXformMath.h"

namespace
{
	FVector ProjectOntoAllowedSubspace(const FVector& V, const FXConstraint& C)
	{
		if (C.Axis == EXAxis::Free)
		{
			return V;
		}

		const FVector Axis = C.AxisDir().GetSafeNormal();
		if (Axis.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}

		const FVector Along = FVector::DotProduct(V, Axis) * Axis;
		return C.bPlane ? (V - Along) : Along;
	}
}

double FBlenderXformSurfaceSnap::MinProjection(const FBox& Bounds, const FVector& UnitDirection)
{
	const FVector N = UnitDirection.GetSafeNormal();
	if (!Bounds.IsValid || N.IsNearlyZero())
	{
		return 0.0;
	}

	const FVector MinCorner(
		N.X >= 0.0 ? Bounds.Min.X : Bounds.Max.X,
		N.Y >= 0.0 ? Bounds.Min.Y : Bounds.Max.Y,
		N.Z >= 0.0 ? Bounds.Min.Z : Bounds.Max.Z);
	return FVector::DotProduct(MinCorner, N);
}

FVector FBlenderXformSurfaceSnap::SolveMoveDelta(const FVector& FallbackMoveDelta,
                                                 const FVector& StartPivot,
                                                 const FBox& StartBounds,
                                                 const FXConstraint& Constraint,
                                                 const FXTuning& Tuning,
                                                 const FXSurfaceHit& Hit,
                                                 double SurfaceGap,
                                                 bool& bOutApplied)
{
	bOutApplied = false;
	const FVector N = Hit.Normal.GetSafeNormal();
	if (!Hit.bValid || !StartBounds.IsValid || N.IsNearlyZero())
	{
		return FallbackMoveDelta;
	}

	const double MinStartProjection = MinProjection(StartBounds, N);
	const double PivotSupport = FVector::DotProduct(StartPivot, N) - MinStartProjection;
	const FVector SurfaceTargetDelta = Hit.Point + N * (PivotSupport + SurfaceGap) - StartPivot;

	// Grid controls the tangential arrangement; the contact correction below wins along the surface
	// normal when an arbitrary plane cannot also lie exactly on the Cartesian grid.
	FVector Result = FBlenderXformMath::ConstrainAndSnapMove(SurfaceTargetDelta, Constraint, Tuning);

	const FVector AllowedNormal = ProjectOntoAllowedSubspace(N, Constraint);
	const double Denominator = FVector::DotProduct(AllowedNormal, N);
	if (Denominator <= 1e-8)
	{
		return FallbackMoveDelta;
	}

	const double RequiredNormalDelta = FVector::DotProduct(Hit.Point, N) + SurfaceGap - MinStartProjection;
	const double Remaining = RequiredNormalDelta - FVector::DotProduct(Result, N);
	Result += AllowedNormal * (Remaining / Denominator);

	bOutApplied = true;
	return Result;
}
