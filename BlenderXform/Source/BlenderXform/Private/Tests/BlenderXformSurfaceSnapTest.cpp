#include "Misc/AutomationTest.h"
#include "BlenderXformSurfaceSnap.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FXSurfaceHit SurfaceHit(const FVector& Point, const FVector& Normal)
	{
		FXSurfaceHit Hit;
		Hit.bValid = true;
		Hit.Point = Point;
		Hit.Normal = Normal;
		return Hit;
	}

	bool ContactMatches(const FBox& StartBounds, const FVector& Delta, const FVector& Point,
	                    const FVector& Normal, double Gap, double Tolerance = 1e-3)
	{
		const FVector N = Normal.GetSafeNormal();
		const double MinAfter = FBlenderXformSurfaceSnap::MinProjection(StartBounds, N) + FVector::DotProduct(Delta, N);
		return FMath::IsNearlyEqual(MinAfter, FVector::DotProduct(Point, N) + Gap, Tolerance);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformSurfaceSnapFreeTest, "BlenderXform.SurfaceSnap.FreeAndDiagonal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformSurfaceSnapFreeTest::RunTest(const FString&)
{
	const FBox Bounds(FVector(-50, -50, -50), FVector(50, 50, 50));
	FXConstraint C;
	FXTuning T;
	bool bApplied = false;
	const FVector Delta = FBlenderXformSurfaceSnap::SolveMoveDelta(
		FVector(7, 8, 9), FVector::ZeroVector, Bounds, C, T,
		SurfaceHit(FVector::ZeroVector, FVector::UpVector), 0.1, bApplied);

	TestTrue(TEXT("free surface snap applies"), bApplied);
	TestTrue(TEXT("cube rests 0.1cm above floor"), Delta.Equals(FVector(0, 0, 50.1), 1e-3));
	TestTrue(TEXT("free result satisfies contact plane"),
		ContactMatches(Bounds, Delta, FVector::ZeroVector, FVector::UpVector, 0.1));

	const FVector DiagonalNormal = FVector(1, 1, 0).GetSafeNormal();
	const FVector DiagonalPoint(100, 20, 30);
	const FVector DiagonalDelta = FBlenderXformSurfaceSnap::SolveMoveDelta(
		FVector::ZeroVector, FVector::ZeroVector, Bounds, C, T,
		SurfaceHit(DiagonalPoint, DiagonalNormal), 0.1, bApplied);
	TestTrue(TEXT("diagonal result satisfies contact plane"),
		bApplied && ContactMatches(Bounds, DiagonalDelta, DiagonalPoint, DiagonalNormal, 0.1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformSurfaceSnapConstraintTest, "BlenderXform.SurfaceSnap.AxisAndPlaneConstraints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformSurfaceSnapConstraintTest::RunTest(const FString&)
{
	const FBox Bounds(FVector(-10, -10, -5), FVector(10, 10, 5));
	FXTuning T;
	bool bApplied = false;

	FXConstraint AxisZ;
	AxisZ.Axis = EXAxis::Z;
	const FVector AxisDelta = FBlenderXformSurfaceSnap::SolveMoveDelta(
		FVector(0, 0, 3), FVector::ZeroVector, Bounds, AxisZ, T,
		SurfaceHit(FVector(100, 200, 10), FVector::UpVector), 0.1, bApplied);
	TestTrue(TEXT("Z constraint reaches horizontal surface"), bApplied);
	TestTrue(TEXT("Z constraint changes only Z"), AxisDelta.Equals(FVector(0, 0, 15.1), 1e-3));

	FXConstraint AxisX;
	AxisX.Axis = EXAxis::X;
	const FVector Fallback(7, 0, 0);
	const FVector ParallelDelta = FBlenderXformSurfaceSnap::SolveMoveDelta(
		Fallback, FVector::ZeroVector, Bounds, AxisX, T,
		SurfaceHit(FVector::ZeroVector, FVector::UpVector), 0.1, bApplied);
	TestFalse(TEXT("axis parallel to surface cannot snap"), bApplied);
	TestTrue(TEXT("parallel axis preserves ordinary move"), ParallelDelta.Equals(Fallback, 1e-3));

	FXConstraint PlaneXY;
	PlaneXY.Axis = EXAxis::Z;
	PlaneXY.bPlane = true;
	const FVector PlaneDelta = FBlenderXformSurfaceSnap::SolveMoveDelta(
		FVector::ZeroVector, FVector::ZeroVector, Bounds, PlaneXY, T,
		SurfaceHit(FVector(100, 20, 30), FVector::ForwardVector), 0.1, bApplied);
	TestTrue(TEXT("XY plane reaches vertical X surface"), bApplied);
	TestTrue(TEXT("plane constraint keeps Z fixed"), PlaneDelta.Equals(FVector(110.1, 20, 0), 1e-3));

	const FVector PlaneFallback(4, 5, 0);
	const FVector PlaneParallel = FBlenderXformSurfaceSnap::SolveMoveDelta(
		PlaneFallback, FVector::ZeroVector, Bounds, PlaneXY, T,
		SurfaceHit(FVector::ZeroVector, FVector::UpVector), 0.1, bApplied);
	TestFalse(TEXT("plane parallel to contact correction cannot snap"), bApplied);
	TestTrue(TEXT("parallel plane preserves ordinary move"), PlaneParallel.Equals(PlaneFallback, 1e-3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformSurfaceSnapGridLocalTest, "BlenderXform.SurfaceSnap.GridAndLocalBasis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformSurfaceSnapGridLocalTest::RunTest(const FString&)
{
	const FBox Bounds(FVector(-5, -5, -5), FVector(5, 5, 5));
	FXTuning T;
	T.bSnap = true;
	T.MoveSnap = 10.0;
	bool bApplied = false;

	FXConstraint Free;
	const FVector GridDelta = FBlenderXformSurfaceSnap::SolveMoveDelta(
		FVector::ZeroVector, FVector::ZeroVector, Bounds, Free, T,
		SurfaceHit(FVector(13, 17, 0), FVector::UpVector), 0.1, bApplied);
	TestTrue(TEXT("grid surface snap applies"), bApplied);
	TestTrue(TEXT("grid controls tangent, contact controls normal"), GridDelta.Equals(FVector(10, 20, 5.1), 1e-3));

	FXConstraint LocalX;
	LocalX.Axis = EXAxis::X;
	LocalX.Orient = EXOrient::Local;
	LocalX.BX = FVector::RightVector;
	LocalX.BY = -FVector::ForwardVector;
	LocalX.BZ = FVector::UpVector;
	T.bSnap = false;
	const FVector LocalDelta = FBlenderXformSurfaceSnap::SolveMoveDelta(
		FVector::ZeroVector, FVector::ZeroVector, Bounds, LocalX, T,
		SurfaceHit(FVector(0, 20, 0), FVector::RightVector), 0.1, bApplied);
	TestTrue(TEXT("local X can contact world Y surface"), bApplied);
	TestTrue(TEXT("local axis direction is respected"), LocalDelta.Equals(FVector(0, 25.1, 0), 1e-3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformSurfaceSnapFallbackTest, "BlenderXform.SurfaceSnap.InvalidInputsFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformSurfaceSnapFallbackTest::RunTest(const FString&)
{
	const FVector Fallback(1, 2, 3);
	FXConstraint C;
	FXTuning T;
	FXSurfaceHit InvalidHit;
	bool bApplied = true;

	TestTrue(TEXT("missing hit returns fallback"),
		FBlenderXformSurfaceSnap::SolveMoveDelta(Fallback, FVector::ZeroVector,
			FBox(FVector(-1), FVector(1)), C, T, InvalidHit, 0.1, bApplied).Equals(Fallback, 1e-3));
	TestFalse(TEXT("missing hit reports not applied"), bApplied);

	TestTrue(TEXT("invalid bounds return fallback"),
		FBlenderXformSurfaceSnap::SolveMoveDelta(Fallback, FVector::ZeroVector,
			FBox(ForceInit), C, T, SurfaceHit(FVector::ZeroVector, FVector::UpVector), 0.1, bApplied).Equals(Fallback, 1e-3));
	TestFalse(TEXT("invalid bounds report not applied"), bApplied);

	TestTrue(TEXT("zero normal returns fallback"),
		FBlenderXformSurfaceSnap::SolveMoveDelta(Fallback, FVector::ZeroVector,
			FBox(FVector(-1), FVector(1)), C, T, SurfaceHit(FVector::ZeroVector, FVector::ZeroVector), 0.1, bApplied).Equals(Fallback, 1e-3));
	TestFalse(TEXT("zero normal reports not applied"), bApplied);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
