#include "BlenderXformEditorSink.h"

#include "Editor.h"
#include "Selection.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "BlenderXform"

FXEditorSink::~FXEditorSink() = default;

bool FXEditorSink::HasSelection() const
{
	return GEditor && GEditor->GetSelectedActors() && GEditor->GetSelectedActors()->Num() > 0;
}

void FXEditorSink::CaptureSelection()
{
	Snapshot.Reset();
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;

	if (GEditor)
	{
		USelection* Sel = GEditor->GetSelectedActors();
		for (FSelectionIterator It(*Sel); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				Snapshot.Add({ Actor, Actor->GetActorTransform() });
				Sum += Actor->GetActorLocation();
				++Count;
			}
		}
	}

	CachedPivot = (Count > 0) ? (Sum / Count) : FVector::ZeroVector;
}

void FXEditorSink::Begin()
{
	CaptureSelection();
	Transaction = MakeUnique<FScopedTransaction>(LOCTEXT("XformTransaction", "Blender Transform"));
	for (const FSnap& Snap : Snapshot)
	{
		if (AActor* Actor = Snap.Actor.Get())
		{
			// The transform lives on the root component, so it must be in the transaction too —
			// modifying only the actor leaves the move/rotate/scale un-undoable.
			Actor->Modify();
			if (USceneComponent* Root = Actor->GetRootComponent())
			{
				Root->Modify();
			}
		}
	}
}

void FXEditorSink::Apply(const FXApplied& In)
{
	const FVector P = In.Pivot;

	for (const FSnap& Snap : Snapshot)
	{
		AActor* Actor = Snap.Actor.Get();
		if (!Actor)
		{
			continue;
		}

		FTransform T = Snap.Xform;

		switch (In.Mode)
		{
			case EXMode::Move:
			{
				T.SetLocation(Snap.Xform.GetLocation() + In.MoveDelta);
				break;
			}
			case EXMode::Scale:
			{
				const FVector F = In.ScaleFac;
				const FVector Rel = Snap.Xform.GetLocation() - P;
				T.SetLocation(P + FVector(Rel.X * F.X, Rel.Y * F.Y, Rel.Z * F.Z));
				T.SetScale3D(Snap.Xform.GetScale3D() * F);
				break;
			}
			case EXMode::Rotate:
			{
				const FQuat Q(In.RotAxis, FMath::DegreesToRadians(In.RotDeg));
				const FVector Rel = Snap.Xform.GetLocation() - P;
				T.SetLocation(P + Q.RotateVector(Rel));
				T.SetRotation((Q * Snap.Xform.GetRotation()).GetNormalized());
				break;
			}
			default:
				break;
		}

		Actor->SetActorTransform(T, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports();
	}
}

void FXEditorSink::Commit()
{
	// Destroying the FScopedTransaction finalizes it into the undo buffer as one step.
	Transaction.Reset();
	Snapshot.Reset();

	// We moved actor transforms directly, so re-seat the editor's transform gizmo on them.
	if (GEditor)
	{
		GEditor->NoteSelectionChange();
		GEditor->RedrawLevelEditingViewports();
	}
}

void FXEditorSink::Cancel()
{
	for (const FSnap& Snap : Snapshot)
	{
		if (AActor* Actor = Snap.Actor.Get())
		{
			Actor->SetActorTransform(Snap.Xform, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	if (Transaction.IsValid())
	{
		Transaction->Cancel(); // discard — nothing lands in the undo buffer
		Transaction.Reset();
	}
	Snapshot.Reset();

	if (GEditor)
	{
		GEditor->NoteSelectionChange();
		GEditor->RedrawLevelEditingViewports();
	}
}

FVector FXEditorSink::Pivot() const
{
	return CachedPivot;
}

void FXEditorSink::ActiveBasis(FVector& X, FVector& Y, FVector& Z) const
{
	X = FVector::ForwardVector;
	Y = FVector::RightVector;
	Z = FVector::UpVector;

	if (GEditor)
	{
		if (AActor* Active = GEditor->GetSelectedActors()->GetTop<AActor>())
		{
			const FQuat Q = Active->GetActorQuat();
			X = Q.GetAxisX();
			Y = Q.GetAxisY();
			Z = Q.GetAxisZ();
		}
	}
}

#undef LOCTEXT_NAMESPACE
