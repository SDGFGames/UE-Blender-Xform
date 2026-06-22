#include "Misc/AutomationTest.h"
#include "BlenderXformMath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformMathConstrainTest, "BlenderXform.Math.ConstrainVector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformMathConstrainTest::RunTest(const FString&)
{
	FXConstraint Free;
	TestTrue(TEXT("free leaves vector unchanged"),
		FBlenderXformMath::ConstrainVector(FVector(3, 4, 5), Free).Equals(FVector(3, 4, 5), 1e-3));

	FXConstraint X; X.Axis = EXAxis::X;
	TestTrue(TEXT("axis X keeps only X"),
		FBlenderXformMath::ConstrainVector(FVector(3, 4, 5), X).Equals(FVector(3, 0, 0), 1e-3));

	FXConstraint PlaneZ; PlaneZ.Axis = EXAxis::Z; PlaneZ.bPlane = true;
	TestTrue(TEXT("plane Z removes Z (free in XY)"),
		FBlenderXformMath::ConstrainVector(FVector(3, 4, 5), PlaneZ).Equals(FVector(3, 4, 0), 1e-3));

	// Local: basis X points down world +Y; constraining keeps only the world-Y component.
	FXConstraint LocalX; LocalX.Axis = EXAxis::X; LocalX.Orient = EXOrient::Local; LocalX.BX = FVector(0, 1, 0);
	TestTrue(TEXT("local X uses the rotated basis"),
		FBlenderXformMath::ConstrainVector(FVector(3, 4, 5), LocalX).Equals(FVector(0, 4, 0), 1e-3));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformMathMoveTest, "BlenderXform.Math.MoveDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformMathMoveTest::RunTest(const FString&)
{
	FXConstraint X; X.Axis = EXAxis::X;

	// Numeric: G X 5 -> exactly (5,0,0)
	TestTrue(TEXT("numeric X 5"),
		FBlenderXformMath::MoveDelta(FVector::ZeroVector, FVector::ZeroVector, X, true, 5.0)
			.Equals(FVector(5, 0, 0), 1e-3));

	// Mouse: world drag (3,4,5) constrained to X -> (3,0,0)
	TestTrue(TEXT("mouse drag constrained to X"),
		FBlenderXformMath::MoveDelta(FVector::ZeroVector, FVector(3, 4, 5), X, false, 0.0)
			.Equals(FVector(3, 0, 0), 1e-3));

	// Free numeric has no direction -> zero
	FXConstraint Free;
	TestTrue(TEXT("free numeric is a no-op"),
		FBlenderXformMath::MoveDelta(FVector::ZeroVector, FVector::ZeroVector, Free, true, 5.0)
			.IsNearlyZero());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformMathScaleTest, "BlenderXform.Math.ScaleFactors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformMathScaleTest::RunTest(const FString&)
{
	FXConstraint Free;
	TestTrue(TEXT("free numeric 2 -> uniform"),
		FBlenderXformMath::ScaleFactors(FVector2D::ZeroVector, FVector2D(1, 0), FVector2D(2, 0), Free, true, 2.0)
			.Equals(FVector(2, 2, 2), 1e-3));

	FXConstraint X; X.Axis = EXAxis::X;
	TestTrue(TEXT("numeric X 2 -> only X"),
		FBlenderXformMath::ScaleFactors(FVector2D::ZeroVector, FVector2D(1, 0), FVector2D(2, 0), X, true, 2.0)
			.Equals(FVector(2, 1, 1), 1e-3));

	// Mouse ratio: cursor twice as far from pivot -> 2x
	TestTrue(TEXT("mouse ratio 2x"),
		FBlenderXformMath::ScaleFactors(FVector2D(0, 0), FVector2D(10, 0), FVector2D(20, 0), Free, false, 0.0)
			.Equals(FVector(2, 2, 2), 1e-3));

	FXConstraint PlaneZ; PlaneZ.Axis = EXAxis::Z; PlaneZ.bPlane = true;
	TestTrue(TEXT("plane Z numeric 2 -> XY scale, Z fixed"),
		FBlenderXformMath::ScaleFactors(FVector2D::ZeroVector, FVector2D(1, 0), FVector2D(2, 0), PlaneZ, true, 2.0)
			.Equals(FVector(2, 2, 1), 1e-3));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformMathRotateTest, "BlenderXform.Math.RotateAngleDeg",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformMathRotateTest::RunTest(const FString&)
{
	// Numeric: R Z 90 -> exactly 90
	TestTrue(TEXT("numeric 90"),
		FMath::IsNearlyEqual(
			FBlenderXformMath::RotateAngleDeg(FVector2D::ZeroVector, FVector2D(1, 0), FVector2D(0, 1),
				FVector(0, 0, 1), FVector(0, 0, -1), true, 90.0),
			90.0, 1e-3));

	// Mouse: a quarter sweep -> magnitude 90 (sign finalized on real hardware)
	const double Geo = FBlenderXformMath::RotateAngleDeg(FVector2D::ZeroVector, FVector2D(1, 0), FVector2D(0, 1),
		FVector(0, 0, 1), FVector(0, 0, -1), false, 0.0);
	TestTrue(TEXT("mouse quarter sweep is 90 deg"), FMath::IsNearlyEqual(FMath::Abs(Geo), 90.0, 1e-3));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
