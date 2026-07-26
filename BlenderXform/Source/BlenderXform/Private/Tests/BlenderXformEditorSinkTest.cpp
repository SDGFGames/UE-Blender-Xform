#include "Misc/AutomationTest.h"
#include "BlenderXformEditorSink.h"
#include "BlenderXformMath.h" // AlignUpToNormalQuat (Apply align test)

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "LandscapeProxy.h"
#include "Editor/Transactor.h" // UTransactor::GetUndoCount (undo-depth assertion in destructor test)
#include "LevelUtils.h"        // FLevelUtils::LockLevel (locked-level denial test)

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
	const FVector Origin(5.0, 6.0, 7.0);

	using P = EBlenderXformSurfaceBoundsPolicy;

	// Collision policy (default, pre-3.2 behavior): collision wins, visual fallback.
	TestTrue(TEXT("Collision: collision bounds win when available"),
		FXEditorSink::PreferredSurfaceBounds(Collision, Visual, Origin, P::Collision).Equals(Collision));
	TestTrue(TEXT("Collision: visual is the no-collision fallback"),
		FXEditorSink::PreferredSurfaceBounds(Invalid, Visual, Origin, P::Collision).Equals(Visual));
	TestFalse(TEXT("Collision: no bounds stays invalid"),
		FXEditorSink::PreferredSurfaceBounds(Invalid, Invalid, Origin, P::Collision).IsValid != 0);

	// Visual policy: visual wins, collision fallback.
	TestTrue(TEXT("Visual: visual bounds win when available"),
		FXEditorSink::PreferredSurfaceBounds(Collision, Visual, Origin, P::Visual).Equals(Visual));
	TestTrue(TEXT("Visual: collision is the no-visual fallback"),
		FXEditorSink::PreferredSurfaceBounds(Collision, Invalid, Origin, P::Visual).Equals(Collision));

	// Origin policy: a zero-volume point at the actor's pivot.
	const FBox OriginBox = FXEditorSink::PreferredSurfaceBounds(Collision, Visual, Origin, P::Origin);
	TestTrue(TEXT("Origin: zero-volume box at the actor location"),
		OriginBox.Equals(FBox(Origin, Origin), 1e-3));
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformEditorSinkTransformabilityRulesTest, "BlenderXform.EditorSink.TransformabilityRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformEditorSinkTransformabilityRulesTest::RunTest(const FString&)
{
	// Task 1.5 / D2: one CheckTransformability covers class, location lock and level lock; a mixed
	// selection with any one denial rejects the whole op (HasSelection strict mode).
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world is available"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;

	AActor* Plain = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(10.0)), SpawnParams);
	AActor* LocLocked = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(20.0)), SpawnParams);
	if (!TestNotNull(TEXT("plain actor spawned"), Plain) || !TestNotNull(TEXT("loc-locked actor spawned"), LocLocked))
	{
		if (Plain) { World->DestroyActor(Plain); }
		if (LocLocked) { World->DestroyActor(LocLocked); }
		return false;
	}

	TestEqual(TEXT("plain editable actor is transformable"),
		FXEditorSink::CheckTransformability(Plain), ETransformDenialReason::None);
	TestTrue(TEXT("CanTransformActor agrees for a plain actor"),
		FXEditorSink::CanTransformActor(Plain));

	// Location-locked actor (Details "Lock Actor Location") must be denied with the right reason.
	LocLocked->SetLockLocation(true);
	TestEqual(TEXT("location-locked actor is denied"),
		FXEditorSink::CheckTransformability(LocLocked), ETransformDenialReason::LocationLocked);
	TestFalse(TEXT("CanTransformActor rejects a location-locked actor"),
		FXEditorSink::CanTransformActor(LocLocked));
	LocLocked->SetLockLocation(false); // restore so teardown is clean

	// Level-locked actor: lock the owning level, expect LevelLocked, then unlock.
	ULevel* Level = Plain->GetLevel();
	if (TestNotNull(TEXT("actor has a level"), Level))
	{
		const bool bWasLocked = Level->bLocked;
		Level->bLocked = true;
		TestEqual(TEXT("actor in a locked level is denied"),
			FXEditorSink::CheckTransformability(Plain), ETransformDenialReason::LevelLocked);
		TestFalse(TEXT("CanTransformActor rejects a level-locked actor"),
			FXEditorSink::CanTransformActor(Plain));
		Level->bLocked = bWasLocked;
	}

	// Null actor and Landscape class stay unsupported.
	TestEqual(TEXT("null actor is unsupported"),
		FXEditorSink::CheckTransformability(nullptr), ETransformDenialReason::UnsupportedClass);
	TestFalse(TEXT("landscape proxy class is still rejected by the class rule"),
		FXEditorSink::CanTransformActorClass(ALandscapeProxy::StaticClass()));

	World->DestroyActor(Plain);
	World->DestroyActor(LocLocked);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformEditorSinkAttachedAltClearTest, "BlenderXform.EditorSink.AttachedAltClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformEditorSinkAttachedAltClearTest::RunTest(const FString&)
{
	// Task 1.6: Alt+G on an attached actor clears its RELATIVE location (stays put w.r.t. its parent),
	// not its world location (which would jump it to the world origin). Same relative semantics for
	// Alt+S/Alt+R; standalone actors keep clearing world transform.
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world is available"), World))
	{
		return false;
	}

	auto SpawnWithRoot = [&](const FVector& Loc) -> AActor*
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Loc), P);
		if (A)
		{
			UBoxComponent* Box = NewObject<UBoxComponent>(A, NAME_None, RF_Transient);
			A->AddInstanceComponent(Box);
			A->SetRootComponent(Box);
			Box->SetMobility(EComponentMobility::Movable);
			Box->RegisterComponent();
		}
		return A;
	};

	const FVector ParentLoc(500.0, 0.0, 0.0);
	AActor* Parent = SpawnWithRoot(FVector::ZeroVector);
	AActor* Child  = SpawnWithRoot(FVector::ZeroVector);
	if (!TestNotNull(TEXT("parent spawned"), Parent) || !TestNotNull(TEXT("child spawned"), Child))
	{
		if (Parent) { World->DestroyActor(Parent); }
		if (Child)  { World->DestroyActor(Child); }
		return false;
	}
	// SetRootComponent on a freshly-spawned empty actor resets its location to the new root's (origin),
	// so position the parent AFTER its root exists and read the real location back.
	Parent->SetActorLocation(ParentLoc, false, nullptr, ETeleportType::None);
	const FVector RealParentLoc = Parent->GetActorLocation();

	// Attach child to parent with a non-zero relative offset, then Alt+G should zero the RELATIVE
	// location - so the child ends up AT the parent's world location, not at the world origin.
	Child->AttachToActor(Parent, FAttachmentTransformRules::KeepRelativeTransform);
	Child->SetActorRelativeLocation(FVector(100.0, 50.0, 25.0));
	TestTrue(TEXT("child is attached to the parent"), Child->GetAttachParentActor() == Parent);
	const FVector WorldBeforeClear = Child->GetActorLocation(); // == RealParentLoc + (100,50,25)
	TestTrue(TEXT("attached child has a non-origin world position before clear"),
		!WorldBeforeClear.Equals(FVector::ZeroVector, 1.0));

	{
		FXEditorSink Sink;
		// Select ONLY the child so the clear targets it (not the parent).
		GEditor->SelectActor(Parent, false, false);
		GEditor->SelectActor(Child, true, false);
		Sink.ClearComponent(EXMode::Move);
	}

	TestTrue(TEXT("Alt+G on an attached child zeros its RELATIVE location (lands on the parent)"),
		Child->GetActorLocation().Equals(RealParentLoc, 1.0));
	TestFalse(TEXT("Alt+G did not send the attached child to the world origin"),
		Child->GetActorLocation().Equals(FVector::ZeroVector, 1.0));

	GEditor->SelectActor(Child, false, false);
	World->DestroyActor(Child);
	World->DestroyActor(Parent);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformEditorSinkApplyAlignTest, "BlenderXform.EditorSink.ApplyAlign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformEditorSinkApplyAlignTest::RunTest(const FString&)
{
	// Task B: when an FXApplied carries bAlignApplied, the Move path composes the rotation about
	// AlignPivot on top of the translation - the actor's Up follows AlignQuat. Verifies the Apply
	// composition directly (Op wiring is Task C).
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world is available"), World))
	{
		return false;
	}

	auto SpawnWithRoot = [&](const FVector& Loc) -> AActor*
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Loc), P);
		if (A)
		{
			UBoxComponent* Box = NewObject<UBoxComponent>(A, NAME_None, RF_Transient);
			A->AddInstanceComponent(Box);
			A->SetRootComponent(Box);
			Box->SetMobility(EComponentMobility::Movable);
			Box->RegisterComponent();
		}
		return A;
	};

	AActor* Actor = SpawnWithRoot(FVector::ZeroVector);
	if (!TestNotNull(TEXT("actor spawned"), Actor))
	{
		return false;
	}

	FXEditorSink Sink;
	if (GEditor) { GEditor->SelectActor(Actor, true, false); }
	Sink.Begin(); // snapshot the actor + open a transaction

	// No align: a plain move translates only, rotation unchanged (identity).
	const FQuat IdentityRot = Actor->GetActorQuat();
	FXApplied Plain;
	Plain.Mode = EXMode::Move;
	Plain.MoveDelta = FVector(100.0, 0.0, 0.0);
	Sink.Apply(Plain);
	TestTrue(TEXT("plain move translates, no rotation change"),
		Actor->GetActorLocation().Equals(FVector(100.0, 0.0, 0.0), 1.0) &&
		Actor->GetActorQuat().Equals(IdentityRot, 1e-3));

	// Align to a wall (+X normal): Up (0,0,1) -> +X, about the contact point (0,0,0).
	FXApplied Aligned = Plain;
	Aligned.MoveDelta = FVector::ZeroVector;
	Aligned.bAlignApplied = true;
	Aligned.AlignPivot = FVector::ZeroVector;
	Aligned.AlignQuat = FBlenderXformMath::AlignUpToNormalQuat(FVector(1, 0, 0));
	Sink.Apply(Aligned);
	const FVector UpAfter = Actor->GetActorQuat().RotateVector(FVector(0, 0, 1));
	TestTrue(TEXT("align rotates Up onto the wall normal (+X)"),
		UpAfter.Equals(FVector(1, 0, 0), 1e-2));

	// Align about a non-origin pivot: the actor (snapshot at origin) orbits the pivot. With pivot=(50,0,0)
	// and Up->+X (90 deg about Y), the offset (-50,0,0) rotates to (0,0,50), landing at (50,0,50).
	Actor->SetActorLocation(FVector(0, 0, 0), false, nullptr, ETeleportType::None);
	FXApplied AP = Aligned;
	AP.AlignPivot = FVector(50.0, 0.0, 0.0);
	Sink.Apply(AP);
	TestTrue(TEXT("align about a pivot orbits the pivot point"),
		Actor->GetActorLocation().Equals(FVector(50.0, 0.0, 50.0), 1.0));

	if (GEditor) { GEditor->SelectActor(Actor, false, false); }
	World->DestroyActor(Actor);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
