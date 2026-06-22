#pragma once

#include "CoreMinimal.h"

/** Which transform a modal op performs. */
enum class EXMode : uint8 { None, Move, Scale, Rotate };

/** Which axis a constraint locks onto (Free = no constraint). */
enum class EXAxis : uint8 { Free, X, Y, Z };

/** Constraint orientation: world axes vs the active actor's local axes. */
enum class EXOrient : uint8 { Global, Local };

/**
 * A transform constraint. The basis vectors BX/BY/BZ are the world-space directions of the
 * X/Y/Z axes in the active orientation — world unit axes for Global, the active actor's
 * rotated axes for Local. The pure math operates only on these vectors, so Global vs Local
 * is just a matter of which basis the caller fills in.
 */
struct FXConstraint
{
	EXAxis   Axis   = EXAxis::Free;
	bool     bPlane = false; // true => lock the named axis, transform freely in the other two
	EXOrient Orient = EXOrient::Global;
	FVector  BX = FVector::ForwardVector; // (1,0,0)
	FVector  BY = FVector::RightVector;   // (0,1,0)
	FVector  BZ = FVector::UpVector;      // (0,0,1)

	/** Unit direction of the constrained single axis (X/Y/Z) in the active basis; zero when Free. */
	FVector AxisDir() const
	{
		switch (Axis)
		{
			case EXAxis::X: return BX;
			case EXAxis::Y: return BY;
			case EXAxis::Z: return BZ;
			default:        return FVector::ZeroVector;
		}
	}

	bool HasConstraint() const { return Axis != EXAxis::Free; }
};

/**
 * Live feel knobs that modulate ONLY mouse-driven magnitude (numeric entry stays exact, as in
 * Blender). The pure math takes this by const-ref so the modal feel is fully unit-testable.
 *   - Sensitivity scales how far a drag moves/rotates (translate & scale share MouseSensitivity).
 *   - bPrecision (Shift held) multiplies by PrecisionScale for fine control.
 *   - bSnap (Ctrl held) quantizes the result: MoveSnap (world units), RotateSnapDeg, ScaleSnap.
 * Default-constructed = identity (sensitivity 1, no precision, no snap) => zero behavior change,
 * so existing callers/tests are unaffected.
 */
struct FXTuning
{
	double MouseSensitivity  = 1.0;  // translate + scale drag gain
	double RotateSensitivity = 1.0;  // rotate drag gain
	bool   bPrecision        = false;
	double PrecisionScale    = 0.1;  // Shift: 10x finer
	bool   bSnap             = false;
	double MoveSnap          = 10.0; // world units (cm) per step
	double RotateSnapDeg     = 5.0;  // degrees per step (Blender default)
	double ScaleSnap         = 0.1;  // factor increment per step

	/** Combined mouse gain for translate/scale (sensitivity, optionally fined by precision). */
	double MoveGain() const  { return MouseSensitivity  * (bPrecision ? PrecisionScale : 1.0); }
	/** Combined mouse gain for rotate. */
	double RotGain() const   { return RotateSensitivity * (bPrecision ? PrecisionScale : 1.0); }
};
