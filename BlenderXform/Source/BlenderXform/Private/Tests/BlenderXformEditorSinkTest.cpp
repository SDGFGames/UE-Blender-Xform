#include "Misc/AutomationTest.h"
#include "BlenderXformEditorSink.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "LandscapeProxy.h"
#include "Editor/Transactor.h" // UTransactor::GetUndoCount (undo-depth assertion in destructor test)

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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformEditorSinkDestructorCancelsOpenTransactionTest, "BlenderXform.EditorSink.DestructorCancelsOpenTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformEditorSinkDestructorCancelsOpenTransactionTest::RunTest(const FString&)
{
	// Task 1.4: if a sink is torn down while an op's transaction is still open (module unload / editor
	// exit / Live Coding), the destructor must CANCEL it - not let FScopedTransaction's destructor
	// silently finalize it into the undo buffer. We observe undo depth: Begin opens a transaction, but
	// destroying the sink without Commit must leave the undo stack unchanged.
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world is available"), World))
	{
		return false;
	}

	const int32 UndoBefore = GEditor->Trans ? GEditor->Trans->GetUndoCount() : 0;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(100.0)), SpawnParams);
	if (!TestNotNull(TEXT("temporary actor spawned"), Actor))
	{
		return false;
	}
	const FVector OriginalLocation = Actor->GetActorLocation();

	// Begin() snapshots the SELECTED actors, so select one or the snapshot (and the transaction payload)
	// would be empty and the test wouldn't exercise the cancel-on-teardown path.
	if (GEditor)
	{
		GEditor->SelectActor(Actor, /*bInSelected*/ true, /*bNotify*/ false);
	}

	{
		FXEditorSink Sink;          // Begin opens a FScopedTransaction + snapshots the actor
		Sink.Begin();
		TestTrue(TEXT("sink reports a live snapshot after Begin"), Sink.HasLiveSnapshot());
		// Sink destroyed here WITH the transaction still open -> destructor must cancel it.
	}

	// Deselect so destroying the actor below doesn't trip selection-related teardown.
	if (GEditor)
	{
		GEditor->SelectActor(Actor, /*bInSelected*/ false, /*bNotify*/ false);
	}

	const int32 UndoAfter = GEditor->Trans ? GEditor->Trans->GetUndoCount() : 0;
	TestEqual(TEXT("tearing down an open transaction adds no undo item (it was cancelled, not finalized)"),
		UndoAfter, UndoBefore);
	TestTrue(TEXT("actor transform is unchanged after the open op was cancelled on teardown"),
		Actor->GetActorLocation().Equals(OriginalLocation, 1e-2));

	World->DestroyActor(Actor);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
