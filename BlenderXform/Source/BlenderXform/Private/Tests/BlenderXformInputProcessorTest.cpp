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

#endif // WITH_DEV_AUTOMATION_TESTS
