#include "Misc/AutomationTest.h"
#include "BlenderXformEditorSink.h"

#include "GameFramework/Actor.h"
#include "LandscapeProxy.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformEditorSinkRejectsLandscapeProxyTest, "BlenderXform.EditorSink.RejectsLandscapeProxy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformEditorSinkRejectsLandscapeProxyTest::RunTest(const FString&)
{
	TestTrue(TEXT("ordinary actors can be transformed"),
		FXEditorSink::CanTransformActorClass(AActor::StaticClass()));
	TestFalse(TEXT("landscape proxies are excluded from modal transforms"),
		FXEditorSink::CanTransformActorClass(ALandscapeProxy::StaticClass()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
