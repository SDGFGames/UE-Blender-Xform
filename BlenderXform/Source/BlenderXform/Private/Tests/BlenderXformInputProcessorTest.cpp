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

#endif // WITH_DEV_AUTOMATION_TESTS
