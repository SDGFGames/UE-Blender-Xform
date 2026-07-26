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

FVector FBlenderXformMath::ConstrainAndSnapMove(const FVector& Raw, const FXConstraint& C, const FXTuning& T)
{
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

	const double CX = (C.Axis == EXAxis::X) ? 0.0 : SnapTo(FVector::DotProduct(Raw, BX), T.MoveSnap);
	const double CY = (C.Axis == EXAxis::Y) ? 0.0 : SnapTo(FVector::DotProduct(Raw, BY), T.MoveSnap);
	const double CZ = (C.Axis == EXAxis::Z) ? 0.0 : SnapTo(FVector::DotProduct(Raw, BZ), T.MoveSnap);
	return CX * BX + CY * BY + CZ * BZ;
}

FVector FBlenderXformMath::RayPlaneIntersect(const FVector& RayOrigin, const FVector& RayDir,
                                             const FVector& PlanePoint, const FVector& PlaneNormal, bool& bOutValid)
{
	const double Denom = FVector::DotProduct(RayDir, PlaneNormal);
	if (FMath::Abs(Denom) < 1e-6)
	{
		bOutValid = false; // ray parallel to the plane
		return PlanePoint;
	}
	const double T = FVector::DotProduct(PlanePoint - RayOrigin, PlaneNormal) / Denom;
	if (T < 0.0)
	{
		bOutValid = false; // hit is behind the ray origin (e.g. pivot plane behind the camera)
		return PlanePoint;
	}
	bOutValid = true;
	return RayOrigin + RayDir * T;
}

double FBlenderXformMath::RayAxisClosestParam(const FVector& RayOrigin, const FVector& RayDir,
                                              const FVector& AxisPoint, const FVector& AxisDir, bool& bOutValid)
{
	// Closest approach between the ray LINE (P0 + s*D0) and the axis LINE (P1 + t*D1). Solving the
	// 2x2 normal equations gives t = (a*e - b*d) / (a*c - b*b), with the skew-line denominators.
	// Returns the parameter along the axis; invalid when the lines are parallel (Denom ~ 0), i.e. the
	// camera looks straight down the axis and no along-axis motion is visible.
	const FVector D0 = RayDir.GetSafeNormal();
	const FVector D1 = AxisDir.GetSafeNormal();
	if (D0.IsNearlyZero() || D1.IsNearlyZero())
	{
		bOutValid = false;
		return 0.0;
	}
	const double a = FVector::DotProduct(D0, D0);
	const double b = FVector::DotProduct(D0, D1);
	const double c = FVector::DotProduct(D1, D1);
	const FVector W0 = RayOrigin - AxisPoint;
	const double d = FVector::DotProduct(D0, W0);
	const double e = FVector::DotProduct(D1, W0);
	const double Denom = a * c - b * b;
	if (FMath::Abs(Denom) < 1e-6)
	{
		bOutValid = false; // ray parallel to the axis (looking straight down it)
		return 0.0;
	}
	bOutValid = true;
	return (a * e - b * d) / Denom;
}

bool FBlenderXformMath::MoveWorldPoints(const FVector& StartOrigin, const FVector& StartDir,
                                        const FVector& NowOrigin, const FVector& NowDir,
                                        const FVector& Pivot, const FVector& CamFwd,
                                        const FXConstraint& C, FVector& OutStart, FVector& OutNow)
{
	OutStart = FVector::ZeroVector;
	OutNow = FVector::ZeroVector;

	const FVector ViewN = CamFwd.GetSafeNormal();
	if (ViewN.IsNearlyZero())
	{
		return false;
	}

	if (C.Axis == EXAxis::Free)
	{
		// Free: the view-facing plane through the pivot (Blender's default grab plane).
		bool bOK = false;
		OutStart = RayPlaneIntersect(StartOrigin, StartDir, Pivot, ViewN, bOK);
		if (!bOK) { return false; }
		OutNow = RayPlaneIntersect(NowOrigin, NowDir, Pivot, ViewN, bOK);
		return bOK;
	}

	const FVector AxisN = C.AxisDir().GetSafeNormal();
	if (AxisN.IsNearlyZero())
	{
		return false;
	}

	if (C.bPlane)
	{
		// Plane: the constraint plane (normal = locked axis) through the pivot. Moving the mouse maps to
		// motion IN that plane, so the locked-axis component is ~0 and ConstrainVector/snap clean up.
		bool bOK = false;
		OutStart = RayPlaneIntersect(StartOrigin, StartDir, Pivot, AxisN, bOK);
		if (!bOK) { return false; }
		OutNow = RayPlaneIntersect(NowOrigin, NowDir, Pivot, AxisN, bOK);
		return bOK;
	}

	// Single-axis: closest approach of each ray to the axis line. Unlike the view-plane deproject, this
	// stays well-conditioned when the axis points near the camera (only the exactly-parallel case bails).
	bool bOK = false;
	const double tStart = RayAxisClosestParam(StartOrigin, StartDir, Pivot, AxisN, bOK);
	if (!bOK) { return false; }
	const double tNow = RayAxisClosestParam(NowOrigin, NowDir, Pivot, AxisN, bOK);
	if (!bOK) { return false; }
	OutStart = Pivot + AxisN * tStart;
	OutNow = Pivot + AxisN * tNow;
	return true;
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
	return ConstrainAndSnapMove(Raw, C, T);
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

FMatrix FBlenderXformMath::ScaleMatrix(const FXConstraint& C, double Factor)
{
	// Factors along the constraint frame's three axes.
	double FX = 1.0, FY = 1.0, FZ = 1.0;
	if (C.Axis == EXAxis::Free)
	{
		FX = FY = FZ = Factor;
	}
	else if (!C.bPlane)
	{
		if      (C.Axis == EXAxis::X) { FX = Factor; }
		else if (C.Axis == EXAxis::Y) { FY = Factor; }
		else                          { FZ = Factor; }
	}
	else
	{
		// plane: the two non-locked axes scale, the named one stays 1
		FX = (C.Axis == EXAxis::X) ? 1.0 : Factor;
		FY = (C.Axis == EXAxis::Y) ? 1.0 : Factor;
		FZ = (C.Axis == EXAxis::Z) ? 1.0 : Factor;
	}

	const FVector AX = C.BX.GetSafeNormal();
	const FVector AY = C.BY.GetSafeNormal();
	const FVector AZ = C.BZ.GetSafeNormal();

	// S = sum_i F_i (A_i (x) A_i): scale by F_i along each orthonormal frame axis, expressed in world
	// space. Symmetric, so it's identical under either matrix-vector convention.
	FMatrix M = FMatrix::Identity;
	for (int32 R = 0; R < 3; ++R)
	{
		for (int32 Col = 0; Col < 3; ++Col)
		{
			M.M[R][Col] = FX * AX[R] * AX[Col] + FY * AY[R] * AY[Col] + FZ * AZ[R] * AZ[Col];
		}
	}
	return M;
}

FTransform FBlenderXformMath::ScaleTransform(const FTransform& Start, const FVector& Pivot, const FMatrix& WorldScale)
{
	const FVector Rel = Start.GetLocation() - Pivot;
	const FVector NewLoc = Pivot + WorldScale.TransformVector(Rel);

	// Apply the object's transform, then the world scale (linear parts only), and decompose back.
	FMatrix Linear = Start.ToMatrixWithScale();
	Linear.SetOrigin(FVector::ZeroVector);
	const FMatrix NewLinear = Linear * WorldScale;

	FTransform Out(NewLinear);
	Out.SetLocation(NewLoc);
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

double FBlenderXformMath::WrapDeltaDeg(double PrevAngleDeg, double NowAngleDeg)
{
	// Smallest signed turn from Prev to Now. Wrapping once into (-180,180] is exact because the true
	// per-frame motion is always < 180 deg; anything outside that is the seam crossing.
	double Delta = NowAngleDeg - PrevAngleDeg;
	while (Delta > 180.0) { Delta -= 360.0; }
	while (Delta <= -180.0) { Delta += 360.0; }
	return Delta;
}
