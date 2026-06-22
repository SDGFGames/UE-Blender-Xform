#pragma once

#include "CoreMinimal.h"
#include "BlenderXformTypes.h"

/**
 * Pure transform math for Blender-style ops. No editor state, no side effects — every input is
 * passed in (camera/view work, deprojection, and screen projection are done by the caller), so
 * these functions are deterministic and unit-testable without a running editor.
 *
 * Global vs Local is expressed entirely through FXConstraint's basis vectors (BX/BY/BZ): the
 * caller fills world unit axes for Global, or the active actor's rotated axes for Local. The math
 * is identical either way.
 */
struct FBlenderXformMath
{
	/**
	 * Apply a constraint to a world vector:
	 *   Free        -> V unchanged
	 *   single axis -> component along that axis only:  (V·dir)·dir
	 *   plane       -> the locked axis removed:         V - (V·dir)·dir
	 */
	static FVector ConstrainVector(const FVector& V, const FXConstraint& C);

	/**
	 * World translation delta for a Move op.
	 *   numeric + single axis -> NumericValue * axisDir   (exact, e.g. G X 5 -> (5,0,0))
	 *   mouse                 -> ConstrainVector(WorldNow - WorldStart, C) * gain, optionally snapped
	 * Numeric with no single axis (Free or plane) has no unique direction -> zero (v1: numeric needs an axis).
	 * Tuning (sensitivity/precision/snap) modulates the MOUSE path only; numeric stays exact. Snap
	 * quantizes along the constraint (single axis -> step along the axis; plane -> step on each free
	 * basis axis; free -> step on each world component), so Local snapping follows the rotated basis.
	 */
	static FVector MoveDelta(const FVector& WorldStart, const FVector& WorldNow,
	                         const FXConstraint& C, bool bNumeric, double NumericValue,
	                         const FXTuning& T = FXTuning());

	/**
	 * Per-axis scale multipliers (1 on axes that don't scale). Applied to an actor's relative
	 * (local) scale, matching UE's own gizmo behavior.
	 *   factor f: numeric -> NumericValue;  mouse -> |NowS-PivotS| / |StartS-PivotS|  (screen space)
	 *   Free  -> (f,f,f);  single axis -> f on that axis;  plane -> f on the two non-locked axes.
	 * Tuning (mouse path): sensitivity/precision scale the deviation from 1 (f = 1 + (raw-1)*gain);
	 * snap quantizes f to ScaleSnap.
	 */
	static FVector ScaleFactors(const FVector2D& PivotS, const FVector2D& StartS, const FVector2D& NowS,
	                            const FXConstraint& C, bool bNumeric, double NumericValue,
	                            const FXTuning& T = FXTuning());

	/**
	 * Signed rotation angle in degrees about the constrained axis.
	 *   numeric -> NumericValue
	 *   mouse   -> signed screen-space sweep of (NowS-PivotS) relative to (StartS-PivotS),
	 *              sign corrected by whether AxisWorld faces the camera (CamForward).
	 * Tuning (mouse path): sensitivity/precision scale the angle; snap quantizes to RotateSnapDeg.
	 */
	static double RotateAngleDeg(const FVector2D& PivotS, const FVector2D& StartS, const FVector2D& NowS,
	                             const FVector& AxisWorld, const FVector& CamForward,
	                             bool bNumeric, double NumericValue,
	                             const FXTuning& T = FXTuning());
};
