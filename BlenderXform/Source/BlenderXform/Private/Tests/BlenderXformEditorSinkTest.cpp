#include "Misc/AutomationTest.h"
#include "BlenderXformEditorSink.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformEditorSinkSurfaceBoundsPolicyTest, "BlenderXform.EditorSink.SurfaceBoundsPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformEditorSinkSurfaceBoundsPolicyTest::RunTest(const FString&)
{
	const FBox Collision(FVector(-1, -2, -3), FVector(1, 2, 3));
	const FBox Visual(FVector(-10), FVector(10));
	const FBox Invalid(ForceInit);

	TestTrue(TEXT("collision bounds win when available"),
		FXEditorSink::PreferredSurfaceBounds(Collision, Visual).Equals(Collision));
	TestTrue(TEXT("visual bounds are the no-collision fallback"),
		FXEditorSink::PreferredSurfaceBounds(Invalid, Visual).Equals(Visual));
	TestFalse(TEXT("no primitive bounds stays invalid"),
		FXEditorSink::PreferredSurfaceBounds(Invalid, Invalid).IsValid != 0);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformEditorSinkStaticTraceTest, "BlenderXform.EditorSink.StaticSurfaceTrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformEditorSinkStaticTraceTest::RunTest(const FString&)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world is available"), World))
	{
		return false;
	}

	const FVector TargetLocation(123456.0, 234567.0, 345678.0);
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	AActor* Target = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(TargetLocation), SpawnParams);
	if (!TestNotNull(TEXT("temporary trace target spawned"), Target))
	{
		return false;
	}

	UBoxComponent* Box = NewObject<UBoxComponent>(Target, NAME_None, RF_Transient);
	Target->AddInstanceComponent(Box);
	Target->SetRootComponent(Box);
	Box->SetMobility(EComponentMobility::Static);
	Box->SetBoxExtent(FVector(10.0));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->SetWorldLocation(TargetLocation);
	Box->RegisterComponent();

	FXEditorSink Sink;
	FXSurfaceHit Hit;
	const bool bHit = Sink.TraceStaticSurface(World, TargetLocation - FVector(100, 0, 0), FVector::ForwardVector, Hit);
	TestTrue(TEXT("static WorldStatic box is traced"), bHit && Hit.bValid);
	TestTrue(TEXT("trace returns the near X face"), FMath::IsNearlyEqual(Hit.Point.X, TargetLocation.X - 10.0, 1.0));
	TestTrue(TEXT("normal faces back toward the ray"), Hit.Normal.Equals(-FVector::ForwardVector, 1e-3));

	World->DestroyActor(Target);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
