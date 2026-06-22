#include "Misc/AutomationTest.h"
#include "BlenderXformOp.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Records what the op hands it, so tests can assert on the computed previews and lifecycle. */
	class FFakeSink : public IXApplySink
	{
	public:
		TArray<FXApplied> Applies;
		int32 Begins = 0, Commits = 0, Cancels = 0;

		virtual void Begin() override { ++Begins; }
		virtual void Apply(const FXApplied& A) override { Applies.Add(A); }
		virtual void Commit() override { ++Commits; }
		virtual void Cancel() override { ++Cancels; }
		virtual FVector Pivot() const override { return FVector::ZeroVector; }
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

#endif // WITH_DEV_AUTOMATION_TESTS
