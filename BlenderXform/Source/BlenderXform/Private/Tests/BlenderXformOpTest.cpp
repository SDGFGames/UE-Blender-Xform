#include "Misc/AutomationTest.h"
#include "BlenderXformOp.h"
#include "BlenderXformSurfaceSnap.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Records what the op hands it, so tests can assert on the computed previews and lifecycle. */
	class FFakeSink : public IXApplySink
	{
	public:
		TArray<FXApplied> Applies;
		int32 Begins = 0, Commits = 0, Cancels = 0, Duplicates = 0;

		virtual void Begin() override { ++Begins; }
		virtual void BeginDuplicate() override { ++Duplicates; }
		virtual void Apply(const FXApplied& A) override { Applies.Add(A); }
		virtual void Commit() override { ++Commits; }
		virtual void Cancel() override { ++Cancels; }
		virtual FVector Pivot() const override { return FVector::ZeroVector; }
		virtual FBox SurfaceBounds() const override { return FBox(FVector(-5), FVector(5)); }
		virtual void ActiveBasis(FVector& X, FVector& Y, FVector& Z) const override
		{
			X = FVector(1, 0, 0); Y = FVector(0, 1, 0); Z = FVector(0, 0, 1);
		}
		const FXApplied& Last() const { return Applies.Last(); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformOpMoveNumericTest, "BlenderXform.Op.MoveNumericCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformOpMoveNumericTest::RunTest(const FString&)
{
	FFakeSink S;
	FBlenderXformOp Op;
	Op.Begin(EXMode::Move, S);
	Op.CycleAxis(EXAxis::X, false);
	Op.PushDigit(TEXT('5'));
	Op.Commit();

	TestEqual(TEXT("snapshot taken once"), S.Begins, 1);
	TestTrue(TEXT("G X 5 -> +5 on X"), S.Last().MoveDelta.Equals(FVector(5, 0, 0), 1e-3));
	TestEqual(TEXT("committed"), S.Commits, 1);
	TestEqual(TEXT("not cancelled"), S.Cancels, 0);
	TestFalse(TEXT("op ended"), Op.IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformOpCancelTest, "BlenderXform.Op.Cancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformOpCancelTest::RunTest(const FString&)
{
	FFakeSink S;
	FBlenderXformOp Op;
	Op.Begin(EXMode::Move, S);
	Op.Cancel();

	TestEqual(TEXT("cancelled"), S.Cancels, 1);
	TestEqual(TEXT("not committed"), S.Commits, 0);
	TestFalse(TEXT("op ended"), Op.IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformOpAxisCycleTest, "BlenderXform.Op.AxisCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformOpAxisCycleTest::RunTest(const FString&)
{
	FFakeSink S;
	FBlenderXformOp Op;
	Op.Begin(EXMode::Move, S);

	Op.CycleAxis(EXAxis::X, false);
	TestTrue(TEXT("1st press -> Global X"),
		Op.Constraint().Axis == EXAxis::X && Op.Constraint().Orient == EXOrient::Global);

	Op.CycleAxis(EXAxis::X, false);
	TestTrue(TEXT("2nd press -> Local X"),
		Op.Constraint().Axis == EXAxis::X && Op.Constraint().Orient == EXOrient::Local);

	Op.CycleAxis(EXAxis::X, false);
	TestTrue(TEXT("3rd press -> Free"), Op.Constraint().Axis == EXAxis::Free);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformOpTuningTest, "BlenderXform.Op.Tuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformOpTuningTest::RunTest(const FString&)
{
	// Sensitivity flows through the modal path: G X, 2x sensitivity, drag 5 -> 10.
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Move, S);
		Op.CycleAxis(EXAxis::X, false);
		FXTuning T; T.MouseSensitivity = 2.0;
		Op.SetTuning(T);
		Op.UpdateFromScreen(FVector::ZeroVector, FVector(5, 0, 0),
			FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector(0, 0, -1));
		TestTrue(TEXT("sensitivity doubles the drag"), S.Last().MoveDelta.Equals(FVector(10, 0, 0), 1e-3));
	}

	// Ctrl-snap flows through: free drag (12,7,-16) on a 10 grid -> (10,10,-20).
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Move, S);
		FXTuning T; T.bSnap = true; T.MoveSnap = 10.0;
		Op.SetTuning(T);
		Op.UpdateFromScreen(FVector::ZeroVector, FVector(12, 7, -16),
			FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector(0, 0, -1));
		TestTrue(TEXT("snap quantizes the modal move"), S.Last().MoveDelta.Equals(FVector(10, 10, -20), 1e-3));
	}

	// Typed value stays exact even with loud tuning set.
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Move, S);
		Op.CycleAxis(EXAxis::X, false);
		FXTuning T; T.MouseSensitivity = 9.0; T.bSnap = true; T.MoveSnap = 7.0;
		Op.SetTuning(T);
		Op.PushDigit(TEXT('5'));
		Op.Commit();
		TestTrue(TEXT("numeric ignores tuning"), S.Last().MoveDelta.Equals(FVector(5, 0, 0), 1e-3));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformOpSkipsUnchangedApplyTest, "BlenderXform.Op.SkipsUnchangedApply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformOpSkipsUnchangedApplyTest::RunTest(const FString&)
{
	FFakeSink S;
	FBlenderXformOp Op;
	Op.Begin(EXMode::Move, S);
	Op.CycleAxis(EXAxis::X, false);

	const int32 AppliesBefore = S.Applies.Num();
	Op.UpdateFromScreen(FVector::ZeroVector, FVector(5, 0, 0),
		FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector(0, 0, -1));
	const int32 AppliesAfterFirstMove = S.Applies.Num();
	Op.UpdateFromScreen(FVector::ZeroVector, FVector(5, 0, 0),
		FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector(0, 0, -1));

	TestTrue(TEXT("first changed cursor applies"), AppliesAfterFirstMove > AppliesBefore);
	TestEqual(TEXT("unchanged cursor does not re-apply"), S.Applies.Num(), AppliesAfterFirstMove);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformOpScaleRotateTest, "BlenderXform.Op.ScaleAndRotate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformOpScaleRotateTest::RunTest(const FString&)
{
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Scale, S);
		Op.PushDigit(TEXT('2'));
		Op.Commit();
		TestTrue(TEXT("S 2 -> uniform x2"), S.Last().ScaleFac.Equals(FVector(2, 2, 2), 1e-3));
	}
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Rotate, S);
		Op.CycleAxis(EXAxis::Z, false);
		Op.PushDigit(TEXT('9'));
		Op.PushDigit(TEXT('0'));
		Op.Commit();
		TestTrue(TEXT("R Z 90 -> 90 deg"), FMath::IsNearlyEqual(S.Last().RotDeg, 90.0, 1e-3));
		TestTrue(TEXT("about world Z"), S.Last().RotAxis.Equals(FVector(0, 0, 1), 1e-3));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformOpDuplicateTest, "BlenderXform.Op.DuplicateGrab",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformOpDuplicateTest::RunTest(const FString&)
{
	// Shift+D starts a Move op via the sink's DUPLICATE path (not plain Begin); the grab then flows
	// exactly like a normal move, and commit keeps it.
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.BeginDuplicate(S);

		TestEqual(TEXT("duplicate path used"), S.Duplicates, 1);
		TestEqual(TEXT("plain Begin NOT used"), S.Begins, 0);
		TestTrue(TEXT("active in Move mode"), Op.IsActive() && Op.GetMode() == EXMode::Move);

		Op.CycleAxis(EXAxis::X, false);
		Op.PushDigit(TEXT('5'));
		Op.Commit();

		TestTrue(TEXT("grab applies +5 on X to the copy"), S.Last().MoveDelta.Equals(FVector(5, 0, 0), 1e-3));
		TestEqual(TEXT("committed"), S.Commits, 1);
		TestFalse(TEXT("op ended"), Op.IsActive());
	}

	// Cancel routes through the sink's Cancel (where the production sink removes the copy).
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.BeginDuplicate(S);
		Op.Cancel();

		TestEqual(TEXT("cancel reached the sink"), S.Cancels, 1);
		TestEqual(TEXT("not committed"), S.Commits, 0);
		TestFalse(TEXT("op ended"), Op.IsActive());
	}
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformOpSurfaceSnapTest, "BlenderXform.Op.SurfaceSnap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformOpSurfaceSnapTest::RunTest(const FString&)
{
	// Valid hit replaces the ordinary mouse-plane Move delta and advertises the active mode.
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Move, S);
		FXSurfaceSnapState Surface;
		Surface.bEnabled = true;
		Surface.bTraceAttempted = true;
		Surface.Hit.bValid = true;
		Surface.Hit.Point = FVector(13, 17, 0);
		Surface.Hit.Normal = FVector::UpVector;
		Op.UpdateFromScreen(FVector::ZeroVector, FVector(1, 2, 3),
			FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector(0, 0, -1), Surface);
		TestTrue(TEXT("surface hit drives the Move delta"), S.Last().MoveDelta.Equals(FVector(13, 17, 5.1), 1e-3));
		TestTrue(TEXT("surface hit badge"), Op.CursorTag().Contains(TEXT("[surface]")));
		TestFalse(TEXT("surface hit is not a miss"), Op.CursorTag().Contains(TEXT("surface:none")));
	}

	// An attempted miss falls back to ordinary movement and reports the miss without freezing.
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Move, S);
		FXSurfaceSnapState Surface;
		Surface.bEnabled = true;
		Surface.bTraceAttempted = true;
		Op.UpdateFromScreen(FVector::ZeroVector, FVector(4, 5, 6),
			FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector(0, 0, -1), Surface);
		TestTrue(TEXT("surface miss keeps ordinary movement"), S.Last().MoveDelta.Equals(FVector(4, 5, 6), 1e-3));
		TestTrue(TEXT("surface miss badge"), Op.CursorTag().Contains(TEXT("[surface:none]")));
	}

	// Armed-before-first-mouse-move is visible but is not called a miss.
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Move, S);
		FXSurfaceSnapState Surface;
		Surface.bEnabled = true;
		Op.UpdateFromScreen(FVector::ZeroVector, FVector::ZeroVector,
			FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector(0, 0, -1), Surface);
		TestTrue(TEXT("armed surface badge"), Op.CursorTag().Contains(TEXT("[surface]")));
		TestFalse(TEXT("armed state is not a miss"), Op.CursorTag().Contains(TEXT("surface:none")));
	}

	// Numeric movement stays exact even if a valid hit was present.
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Move, S);
		Op.CycleAxis(EXAxis::Z, false);
		FXSurfaceSnapState Surface;
		Surface.bEnabled = true;
		Surface.bTraceAttempted = true;
		Surface.Hit.bValid = true;
		Surface.Hit.Point = FVector(0, 0, 100);
		Surface.Hit.Normal = FVector::UpVector;
		Op.UpdateFromScreen(FVector::ZeroVector, FVector::ZeroVector,
			FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector(0, 0, -1), Surface);
		Op.PushDigit(TEXT('1'));
		Op.PushDigit(TEXT('0'));
		TestTrue(TEXT("numeric Move bypasses surface correction"), S.Last().MoveDelta.Equals(FVector(0, 0, 10), 1e-3));
		TestTrue(TEXT("enabled state remains visible while typing"), Op.CursorTag().Contains(TEXT("[surface]")));
		TestFalse(TEXT("numeric mode is not a surface miss"), Op.CursorTag().Contains(TEXT("surface:none")));
	}

	// Surface state persists outside the Op, but Scale itself never applies or displays it.
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Scale, S);
		FXSurfaceSnapState Surface;
		Surface.bEnabled = true;
		Surface.bTraceAttempted = true;
		Surface.Hit.bValid = true;
		Surface.Hit.Point = FVector(0, 0, 100);
		Surface.Hit.Normal = FVector::UpVector;
		Op.UpdateFromScreen(FVector::ZeroVector, FVector(10, 0, 0),
			FVector2D::ZeroVector, FVector2D(10, 0), FVector2D(20, 0), FVector(0, 0, -1), Surface);
		TestFalse(TEXT("Scale HUD omits surface mode"), Op.CursorTag().Contains(TEXT("surface")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformOpCursorTagTest, "BlenderXform.Op.CursorTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformOpCursorTagTest::RunTest(const FString&)
{
	// Inactive => empty (the HUD then draws neither tag nor hint bar).
	{
		FBlenderXformOp Op;
		TestTrue(TEXT("inactive => empty tag"), Op.CursorTag().IsEmpty());
	}

	// Move + axis X + typed 5 => "X 5|": axis letter, typed buffer, caret; NO mode word, NO separators.
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Move, S);
		Op.CycleAxis(EXAxis::X, false);
		Op.PushDigit(TEXT('5'));
		TestEqual(TEXT("typed move tag"), Op.CursorTag(), FString(TEXT("X 5|")));
	}

	// Live move along X (no numeric) => "X 5.00" (2-dp magnitude).
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Move, S);
		Op.CycleAxis(EXAxis::X, false);
		Op.UpdateFromScreen(FVector::ZeroVector, FVector(5, 0, 0),
			FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector(0, 0, -1));
		TestEqual(TEXT("live move tag"), Op.CursorTag(), FString(TEXT("X 5.00")));
	}

	// Typed scale, free (no axis) => "2|".
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Scale, S);
		Op.PushDigit(TEXT('2'));
		TestEqual(TEXT("typed scale tag"), Op.CursorTag(), FString(TEXT("2|")));
	}

	// Typed rotate about Z => "Z 90|".
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Rotate, S);
		Op.CycleAxis(EXAxis::Z, false);
		Op.PushDigit(TEXT('9'));
		Op.PushDigit(TEXT('0'));
		TestEqual(TEXT("typed rotate tag"), Op.CursorTag(), FString(TEXT("Z 90|")));
	}

	// Live badges: Ctrl-snap + Shift-fine append [snap] [fine], axis prefix preserved.
	{
		FFakeSink S;
		FBlenderXformOp Op;
		Op.Begin(EXMode::Move, S);
		Op.CycleAxis(EXAxis::X, false);
		FXTuning T; T.bSnap = true; T.bPrecision = true;
		Op.SetTuning(T);
		Op.UpdateFromScreen(FVector::ZeroVector, FVector(10, 0, 0),
			FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector(0, 0, -1));
		const FString Tag = Op.CursorTag();
		TestTrue(TEXT("axis kept"), Tag.StartsWith(TEXT("X ")));
		TestTrue(TEXT("snap badge"), Tag.Contains(TEXT("[snap]")));
		TestTrue(TEXT("fine badge"), Tag.Contains(TEXT("[fine]")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
