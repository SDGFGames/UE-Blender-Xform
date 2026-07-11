#pragma once

#include "CoreMinimal.h"
#include "BlenderXformTypes.h"

/** Small, editor-independent surface hit value passed from the viewport trace into the modal op. */
struct FXSurfaceHit
{
	bool bValid = false;
	FVector Point = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
};

/** Per-update surface mode state. The persistent enable flag itself is owned by the input processor. */
struct FXSurfaceSnapState
{
	bool bEnabled = false;
	bool bTraceAttempted = false;
	FXSurfaceHit Hit;
};

/** Pure contact math for cursor-directed surface snapping. */
struct FBlenderXformSurfaceSnap
{
	/** Minimum scalar projection of an axis-aligned box onto UnitDirection. */
	static double MinProjection(const FBox& Bounds, const FVector& UnitDirection);

	/**
	 * Place the translated start bounds against Hit's plane while obeying Constraint and grid snap.
	 * Returns FallbackMoveDelta and bOutApplied=false whenever the contact is not solvable.
	 */
	static FVector SolveMoveDelta(const FVector& FallbackMoveDelta,
	                              const FVector& StartPivot,
	                              const FBox& StartBounds,
	                              const FXConstraint& Constraint,
	                              const FXTuning& Tuning,
	                              const FXSurfaceHit& Hit,
	                              double SurfaceGap,
	                              bool& bOutApplied);
};
