#include "Misc/AutomationTest.h"
#include "BlenderXformHUD.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformHUDHintTextTest, "BlenderXform.HUD.HintTextForViewportWidth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformHUDHintTextTest::RunTest(const FString&)
{
	const FString Wide = FBlenderXformHUD::HintTextForViewportWidth(1400.0f);
	const FString Narrow = FBlenderXformHUD::HintTextForViewportWidth(500.0f);

	TestTrue(TEXT("wide viewports keep the full hint text"), Wide.Contains(TEXT("Shift+axis")));
	TestTrue(TEXT("narrow viewports keep confirm/cancel visible"), Narrow.Contains(TEXT("Enter/LMB")) && Narrow.Contains(TEXT("Esc/RMB")));
	TestTrue(TEXT("narrow hint is shorter"), Narrow.Len() < Wide.Len());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
