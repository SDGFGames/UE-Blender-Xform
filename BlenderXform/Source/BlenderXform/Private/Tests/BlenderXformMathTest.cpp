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

// --- Feel knobs: sensitivity / precision / snap (Blender Ctrl+Shift). Mouse path only; numeric stays exact. ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformMathTuningMoveTest, "BlenderXform.Math.TuningMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformMathTuningMoveTest::RunTest(const FString&)
{
	FXConstraint X; X.Axis = EXAxis::X;

	// Sensitivity 2x doubles a 5-unit drag on X -> 10.
	FXTuning Sens; Sens.MouseSensitivity = 2.0;
	TestTrue(TEXT("sensitivity 2x"),
		FBlenderXformMath::MoveDelta(FVector::ZeroVector, FVector(5, 0, 0), X, false, 0.0, Sens)
			.Equals(FVector(10, 0, 0), 1e-3));

	// Precision (Shift) makes a 10-unit drag 10x finer -> 1.
	FXTuning Prec; Prec.bPrecision = true; Prec.PrecisionScale = 0.1;
	TestTrue(TEXT("precision 0.1x"),
		FBlenderXformMath::MoveDelta(FVector::ZeroVector, FVector(10, 0, 0), X, false, 0.0, Prec)
			.Equals(FVector(1, 0, 0), 1e-3));

	// Snap (Ctrl) on a single axis: 12-unit drag, 10-unit grid -> 10.
	FXTuning Snap; Snap.bSnap = true; Snap.MoveSnap = 10.0;
	TestTrue(TEXT("snap single-axis to grid"),
		FBlenderXformMath::MoveDelta(FVector::ZeroVector, FVector(12, 0, 0), X, false, 0.0, Snap)
			.Equals(FVector(10, 0, 0), 1e-3));

	// Snap on Free move quantizes every world component independently.
	FXConstraint Free;
	TestTrue(TEXT("snap free per-component"),
		FBlenderXformMath::MoveDelta(FVector::ZeroVector, FVector(12, 7, -16), Free, false, 0.0, Snap)
			.Equals(FVector(10, 10, -20), 1e-3));

	// Snap on a local single axis quantizes ALONG the rotated basis, not world axes.
	FXConstraint LocalX; LocalX.Axis = EXAxis::X; LocalX.Orient = EXOrient::Local; LocalX.BX = FVector(0, 1, 0);
	TestTrue(TEXT("snap respects local basis"),
		FBlenderXformMath::MoveDelta(FVector::ZeroVector, FVector(3, 12, 5), LocalX, false, 0.0, Snap)
			.Equals(FVector(0, 10, 0), 1e-3));

	// Numeric is exact regardless of snap/sensitivity.
	FXTuning Loud; Loud.MouseSensitivity = 5.0; Loud.bSnap = true; Loud.MoveSnap = 7.0;
	TestTrue(TEXT("numeric ignores feel knobs"),
		FBlenderXformMath::MoveDelta(FVector::ZeroVector, FVector::ZeroVector, X, true, 5.0, Loud)
			.Equals(FVector(5, 0, 0), 1e-3));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformMathTuningScaleRotateTest, "BlenderXform.Math.TuningScaleRotate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformMathTuningScaleRotateTest::RunTest(const FString&)
{
	FXConstraint Free;

	// Scale sensitivity scales the deviation from 1: raw ratio 2 at 0.5 sensitivity -> 1.5.
	FXTuning Sens; Sens.MouseSensitivity = 0.5;
	TestTrue(TEXT("scale sensitivity dampens factor"),
		FBlenderXformMath::ScaleFactors(FVector2D(0, 0), FVector2D(10, 0), FVector2D(20, 0), Free, false, 0.0, Sens)
			.Equals(FVector(1.5, 1.5, 1.5), 1e-3));

	// Scale snap to 0.1: raw ratio 2.34 -> 2.3.
	FXTuning Snap; Snap.bSnap = true; Snap.ScaleSnap = 0.1;
	TestTrue(TEXT("scale snap to 0.1"),
		FBlenderXformMath::ScaleFactors(FVector2D(0, 0), FVector2D(10, 0), FVector2D(23.4, 0), Free, false, 0.0, Snap)
			.Equals(FVector(2.3, 2.3, 2.3), 1e-3));

	// Rotate sensitivity halves a 90-degree sweep -> 45 (magnitude).
	FXTuning RSens; RSens.RotateSensitivity = 0.5;
	const double Half = FBlenderXformMath::RotateAngleDeg(FVector2D::ZeroVector, FVector2D(1, 0), FVector2D(0, 1),
		FVector(0, 0, 1), FVector(0, 0, -1), false, 0.0, RSens);
	TestTrue(TEXT("rotate sensitivity 0.5"), FMath::IsNearlyEqual(FMath::Abs(Half), 45.0, 1e-3));

	// Rotate snap to 15 degrees: a 50-degree sweep -> 45.
	FXTuning RSnap; RSnap.bSnap = true; RSnap.RotateSnapDeg = 15.0;
	const FVector2D Now50(FMath::Cos(FMath::DegreesToRadians(50.0)), FMath::Sin(FMath::DegreesToRadians(50.0)));
	const double Snapped = FBlenderXformMath::RotateAngleDeg(FVector2D::ZeroVector, FVector2D(1, 0), Now50,
		FVector(0, 0, 1), FVector(0, 0, -1), false, 0.0, RSnap);
	TestTrue(TEXT("rotate snap to 15 deg"), FMath::IsNearlyEqual(FMath::Abs(Snapped), 45.0, 1e-3));

	// Mouse-drag scale must never collapse to zero or mirror-flip (only typed values can, in Blender).
	// Snap that rounds toward 0 floors at one snap step.
	FXTuning ScaleSnap; ScaleSnap.bSnap = true; ScaleSnap.ScaleSnap = 0.1;
	TestTrue(TEXT("snap near pivot floors at one step, not 0"),
		FBlenderXformMath::ScaleFactors(FVector2D(0, 0), FVector2D(100, 0), FVector2D(4, 0), Free, false, 0.0, ScaleSnap)
			.Equals(FVector(0.1, 0.1, 0.1), 1e-3));

	// High sensitivity could drive the factor negative; the drag path floors it positive.
	FXTuning Hot; Hot.MouseSensitivity = 2.0;
	const FVector NoFlip = FBlenderXformMath::ScaleFactors(FVector2D(0, 0), FVector2D(100, 0), FVector2D(30, 0), Free, false, 0.0, Hot);
	TestTrue(TEXT("drag never produces negative/zero scale"), NoFlip.X > 0.0 && NoFlip.X < 0.01);

	// Typed negative IS allowed (Blender mirror via keyboard) and is NOT floored.
	TestTrue(TEXT("typed negative scale flips"),
		FBlenderXformMath::ScaleFactors(FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, Free, true, -1.0, Hot)
			.Equals(FVector(-1, -1, -1), 1e-3));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
