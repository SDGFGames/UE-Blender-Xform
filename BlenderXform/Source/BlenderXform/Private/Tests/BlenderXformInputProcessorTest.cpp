#include "Misc/AutomationTest.h"
#include "BlenderXformInputProcessor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformInputPolicyTest, "BlenderXform.Input.ActiveShortcutPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformInputPolicyTest::RunTest(const FString&)
{
	TestTrue(TEXT("Ctrl+Z is consumed during a modal op"),
		FBlenderXformInputPolicy::ShouldConsumeUnsafeEditorShortcut(EKeys::Z, true, false));
	TestTrue(TEXT("Cmd+Shift+Z is consumed during a modal op"),
		FBlenderXformInputPolicy::ShouldConsumeUnsafeEditorShortcut(EKeys::Z, false, true));
	TestTrue(TEXT("Ctrl+Y is consumed during a modal op"),
		FBlenderXformInputPolicy::ShouldConsumeUnsafeEditorShortcut(EKeys::Y, true, false));
	TestTrue(TEXT("Ctrl+S is consumed during a modal op"),
		FBlenderXformInputPolicy::ShouldConsumeUnsafeEditorShortcut(EKeys::S, true, false));

	TestFalse(TEXT("plain Z remains a modal axis key"),
		FBlenderXformInputPolicy::ShouldConsumeUnsafeEditorShortcut(EKeys::Z, false, false));
	TestFalse(TEXT("unrelated Ctrl chords still pass through"),
		FBlenderXformInputPolicy::ShouldConsumeUnsafeEditorShortcut(EKeys::B, true, false));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformSurfaceSnapInputPolicyTest, "BlenderXform.Input.SurfaceSnapTogglePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformSurfaceSnapInputPolicyTest::RunTest(const FString&)
{
	TestTrue(TEXT("plain C toggles during Move"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::C, false, false, false, false, false));
	TestFalse(TEXT("key repeat cannot toggle repeatedly"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::C, true, false, false, false, false));
	TestFalse(TEXT("idle C remains native UE input"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::None, EKeys::C, false, false, false, false, false));
	TestFalse(TEXT("Scale does not toggle surface mode"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Scale, EKeys::C, false, false, false, false, false));
	TestFalse(TEXT("Rotate does not toggle surface mode"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Rotate, EKeys::C, false, false, false, false, false));
	TestFalse(TEXT("other keys do not toggle"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::V, false, false, false, false, false));
	TestFalse(TEXT("Shift+C is reserved rather than toggling"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::C, false, true, false, false, false));
	TestFalse(TEXT("Ctrl+C remains an editor chord"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::C, false, false, true, false, false));
	TestFalse(TEXT("Alt+C remains an editor chord"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::C, false, false, false, false, true));

	TestFalse(TEXT("same pixel has not begun surface placement"),
		FBlenderXformInputPolicy::HasMovedForSurfaceSnap(FVector2D(10, 20), FVector2D(10.5, 20.5)));
	TestTrue(TEXT("more than one pixel begins surface placement"),
		FBlenderXformInputPolicy::HasMovedForSurfaceSnap(FVector2D(10, 20), FVector2D(11.1, 20)));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformViewportOwnershipPolicyTest, "BlenderXform.Input.ViewportOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformViewportOwnershipPolicyTest::RunTest(const FString&)
{
	// Sentinel addresses stand in for distinct viewport clients; the policy is a pure pointer-identity
	// check and never dereferences them.
	FLevelEditorViewportClient* const VP_A = reinterpret_cast<FLevelEditorViewportClient*>(static_cast<uintptr_t>(0x1000));
	FLevelEditorViewportClient* const VP_B = reinterpret_cast<FLevelEditorViewportClient*>(static_cast<uintptr_t>(0x2000));

	TestTrue(TEXT("the initiating viewport owns itself"),
		FBlenderXformInputPolicy::OwnsViewport(VP_A, VP_A));

	// Task 1.1: once an op starts in VP_A, another viewport client must not become the mouse-update
	// source, and a click in it must not commit/cancel the op.
	TestFalse(TEXT("a different viewport is not owned - cannot become the update source"),
		FBlenderXformInputPolicy::OwnsViewport(VP_A, VP_B));
	TestFalse(TEXT("a click in a non-initiating viewport does not own the op"),
		FBlenderXformInputPolicy::OwnsViewport(VP_A, VP_B));

	TestFalse(TEXT("no candidate viewport is not owned"),
		FBlenderXformInputPolicy::OwnsViewport(VP_A, nullptr));
	TestFalse(TEXT("with no active op (null initiating) nothing is owned"),
		FBlenderXformInputPolicy::OwnsViewport(nullptr, VP_A));
	TestFalse(TEXT("two null viewports own nothing"),
		FBlenderXformInputPolicy::OwnsViewport(nullptr, nullptr));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
