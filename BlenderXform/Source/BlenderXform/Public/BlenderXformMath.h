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

	/** Apply the active constraint and, when enabled, quantize the result to the Move grid. */
	static FVector ConstrainAndSnapMove(const FVector& Raw, const FXConstraint& C, const FXTuning& T);

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
	 * World-space scale matrix for a constraint frame + scalar factor. Scales by Factor along the
	 * constrained frame axes (Free -> all three; single axis -> that one; plane -> the two free ones),
	 * leaving the rest at 1. Because it's built in WORLD space from the frame's basis (world axes for
	 * Global, the actor's rotated axes for Local), a Global single-axis scale on a rotated object
	 * correctly scales along the world axis instead of the object's local axis.
	 */
	static FMatrix ScaleMatrix(const FXConstraint& C, double Factor);

	/**
	 * Apply a world-space scale (from ScaleMatrix) to a Start transform about a world Pivot: scales
	 * the pivot offset and composes the scale onto the transform, then decomposes back to
	 * rotation+scale. Any shear a Global axis-scale induces on a rotated object is dropped — the same
	 * approximation UE's FTransform (and Blender's object-mode T/R/S) inherently makes.
	 */
	static FTransform ScaleTransform(const FTransform& Start, const FVector& Pivot, const FMatrix& WorldScale);

	/**
	 * Intersect a ray (RayOrigin + t*RayDir, t >= 0) with a plane. Sets bOutValid = false and returns
	 * PlanePoint when the ray is parallel to the plane OR the hit is behind the origin (t < 0) — e.g.
	 * the pivot plane is behind the camera, which would otherwise deproject to a garbage world point.
	 */
	static FVector RayPlaneIntersect(const FVector& RayOrigin, const FVector& RayDir,
	                                 const FVector& PlanePoint, const FVector& PlaneNormal, bool& bOutValid);

	/**
	 * Parameter t (point on the axis line = AxisPoint + t*AxisDir) of the closest approach between a
	 * ray and the axis LINE. Used for single-axis Move: the mouse ray's nearest point on the constrained
	 * world axis. Sets bOutValid = false and returns 0.0 when the ray is parallel to the axis (looking
	 * straight down it - no along-axis motion is visible); a NEAR-parallel ray stays finite and valid.
	 * RayDir/AxisDir are normalized internally.
	 */
	static double RayAxisClosestParam(const FVector& RayOrigin, const FVector& RayDir,
	                                  const FVector& AxisPoint, const FVector& AxisDir, bool& bOutValid);

	/**
	 * Per-constraint Start/Now world points for a Move op, from the two mouse rays (Task 2.1):
	 *   Free        -> intersect each ray with the view plane (normal CamFwd, through Pivot)
	 *   Plane       -> intersect each ray with the constraint plane (normal = locked axis, through Pivot)
	 *   Single-axis -> closest approach of each ray to the axis line (Pivot + t*AxisDir)
	 * Returns false (OutStart/OutNow left at zero) if either point is degenerate (ray parallel to its
	 * plane/axis, or the hit is behind the camera). The caller still applies gain + constrain/snap, so
	 * Local basis, precision and grid snap all take effect AFTER this solve.
	 */
	static bool MoveWorldPoints(const FVector& StartOrigin, const FVector& StartDir,
	                            const FVector& NowOrigin, const FVector& NowDir,
	                            const FVector& Pivot, const FVector& CamFwd,
	                            const FXConstraint& C, FVector& OutStart, FVector& OutNow);

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

	/**
	 * Smallest signed delta (degrees, in (-180,180]) from PrevAngle to NowAngle, so a sweep that crosses
	 * the +/-180 seam (179 -> -179) yields +2, not -358. Angles in degrees. Used by the Op to accumulate
	 * mouse rotation frame-to-frame without the +/-180 jump (Task 2.2).
	 */
	static double WrapDeltaDeg(double PrevAngleDeg, double NowAngleDeg);

	/**
	 * Rotation that maps world Up (0,0,1) onto the surface normal N (normalized internally). Used by
	 * surface normal-align (Move + C hold): spin the selection so its Up points along the hit normal.
	 *   N ~ +Up      -> identity
	 *   N ~ -Up      -> 180 deg about world X (deterministic; the roll axis is arbitrary in v1)
	 *   otherwise    -> axis = normalize(Up x N), angle = acos(dot(Up,N))
	 * Returns identity for a zero/invalid normal.
	 */
	static FQuat AlignUpToNormalQuat(const FVector& N);
};
