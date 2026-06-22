#include "BlenderXformEditorSink.h"
#include "BlenderXformMath.h" // FBlenderXformMath::ScaleTransform

#include "Editor.h"
#include "Editor/UnrealEdEngine.h" // UUnrealEdEngine::UpdatePivotLocationForSelection
#include "UnrealEdGlobals.h"       // GUnrealEd
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
	FullSelection.Reset();
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;

	if (GEditor)
	{
		USelection* Sel = GEditor->GetSelectedActors();

		// Collect the selected set first, so we can skip any actor a selected ANCESTOR will move via
		// attachment — transforming both would translate the child twice.
		TSet<AActor*> Selected;
		for (FSelectionIterator It(*Sel); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				Selected.Add(Actor);
			}
		}

		for (AActor* Actor : Selected)
		{
			// Every selected actor counts toward the median pivot and must be re-selected after a
			// cancel — so track them all here, independent of the transform filter below.
			FullSelection.Add(Actor);
			Sum += Actor->GetActorLocation();
			++Count;

			// But don't transform an actor a selected ANCESTOR will already move via attachment —
			// applying to both would translate the child twice.
			bool bAncestorSelected = false;
			for (AActor* P = Actor->GetAttachParentActor(); P; P = P->GetAttachParentActor())
			{
				if (Selected.Contains(P)) { bAncestorSelected = true; break; }
			}
			if (bAncestorSelected)
			{
				continue;
			}

			Snapshot.Add({ Actor, Actor->GetActorTransform() });
		}
	}

	CachedPivot = (Count > 0) ? (Sum / Count) : FVector::ZeroVector;

	// Snapshot the active actor's local axes now (op start) so a Local constraint stays put even as
	// this op rotates the actor — reading GetActorQuat() live would drift mid-op.
	ActiveBX = FVector::ForwardVector;
	ActiveBY = FVector::RightVector;
	ActiveBZ = FVector::UpVector;
	if (GEditor)
	{
		// GetBottom = most recently selected = UE's "active" actor (matches Blender's active-element
		// orientation); GetTop would be the earliest-selected.
		if (AActor* Active = GEditor->GetSelectedActors()->GetBottom<AActor>())
		{
			const FQuat Q = Active->GetActorQuat();
			ActiveBX = Q.GetAxisX();
			ActiveBY = Q.GetAxisY();
			ActiveBZ = Q.GetAxisZ();
		}
	}
}

void FXEditorSink::Begin()
{
	// A fresh op supersedes any still-pending re-selection from a previous cancel.
	PendingReselect.Reset();
	ReselectTicksLeft = 0;

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
				// World-space scale about the pivot (built from the constraint frame), so a Global
				// single-axis scale follows the world axis even on a rotated actor; Local follows the
				// actor's own axes. Replaces the old component-wise Scale3D multiply (always local).
				T = FBlenderXformMath::ScaleTransform(Snap.Xform, P, In.ScaleMat);
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
		// Interactive (non-final) move notification so Blueprint actors, lights, and construction
		// scripts update live during the drag — SetActorTransform alone doesn't trigger them.
		Actor->PostEditMove(false);
	}

	// Make the editor's transform gizmo follow the selection live during the drag. The widget draws
	// at FEditorModeTools::PivotLocation — a cached value that only refreshes on a selection change —
	// so we re-seat it each Apply. UpdatePivotLocationForSelection re-reads the current actor
	// transforms and fires no selection delegates / Details-panel refresh (unlike NoteSelectionChange),
	// so it's cheap enough for the per-mouse-move path; it's also exactly what UE itself calls at the
	// end of its own drags, so the gizmo doesn't jump again on Commit.
	if (GUnrealEd)
	{
		GUnrealEd->SetPivotMovedIndependently(false);
		GUnrealEd->UpdatePivotLocationForSelection();
	}

	if (GEditor)
	{
		// Mid-drag: don't rebuild hit proxies every frame (expensive); pick-accuracy is restored on
		// Commit/Cancel, which redraw with the default (invalidate-hit-proxies = true).
		GEditor->RedrawLevelEditingViewports(/*bInvalidateHitProxies*/ false);
	}
}

void FXEditorSink::Commit()
{
	// Final move notification (bFinished=true) so Blueprints/construction scripts run their full
	// post-edit path; must happen while the transaction is still open and the snapshot is intact.
	for (const FSnap& Snap : Snapshot)
	{
		if (AActor* Actor = Snap.Actor.Get())
		{
			Actor->PostEditMove(true);
		}
	}

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
			Actor->PostEditMove(true); // restored to the original transform — finalize dependent visuals
		}
	}

	if (Transaction.IsValid())
	{
		Transaction->Cancel(); // discard — nothing lands in the undo buffer
		Transaction.Reset();
	}

	// Move the gizmo back onto the restored selection: the drag left UE's editor pivot at the
	// dragged-to location, so without this the transform widget stays where the object was dragged
	// instead of snapping back with it. (On the macOS Esc path DrainReselect's NoteSelectionChange also
	// corrects it, but RMB-cancel — and any case where the native deselect doesn't fire — needs this.)
	if (GUnrealEd)
	{
		GUnrealEd->SetPivotMovedIndependently(false);
		GUnrealEd->UpdatePivotLocationForSelection();
	}

	// Blender keeps the selection after a cancel. On macOS the physical Escape that cancels us is
	// detected by a hardware poll in the processor's Tick, which runs BEFORE Slate pumps key events
	// that frame — so UE's native Escape->"SELECT NONE" (bound on the level viewport) deselects our
	// actors AFTER we cancel, and an immediate re-select here loses the race. Instead, queue the
	// actors and re-assert them over the next few frames from the processor's Tick (DrainReselect),
	// which lands after the native deselect. RMB-cancel consumes its own event (no native deselect),
	// so DrainReselect just sees them still selected and does nothing.
	// Re-select the FULL original selection (including attached children that were filtered out of the
	// transform snapshot) — otherwise the native Escape-deselect leaves those children deselected.
	PendingReselect = FullSelection;
	ReselectTicksLeft = 3;

	Snapshot.Reset();
	FullSelection.Reset();

	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports();
	}
}

void FXEditorSink::DrainReselect()
{
	if (ReselectTicksLeft <= 0)
	{
		return;
	}
	--ReselectTicksLeft;

	if (GEditor)
	{
		USelection* Sel = GEditor->GetSelectedActors();

		// Only re-assert if UE's native Escape-deselect actually dropped our actors. Checking first
		// avoids needless Details-panel churn on the common path (RMB-cancel, or an Escape we already
		// consumed) where nothing was deselected.
		bool bAllSelected = (Sel != nullptr);
		for (const TWeakObjectPtr<AActor>& Weak : PendingReselect)
		{
			AActor* Actor = Weak.Get();
			if (Actor && (!Sel || !Sel->IsSelected(Actor)))
			{
				bAllSelected = false;
				break;
			}
		}

		if (!bAllSelected)
		{
			GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
			for (const TWeakObjectPtr<AActor>& Weak : PendingReselect)
			{
				if (AActor* Actor = Weak.Get())
				{
					GEditor->SelectActor(Actor, /*bInSelected*/ true, /*bNotify*/ false);
				}
			}
			GEditor->NoteSelectionChange();
		}
	}

	if (ReselectTicksLeft <= 0)
	{
		PendingReselect.Reset();
	}
}

FVector FXEditorSink::Pivot() const
{
	return CachedPivot;
}

bool FXEditorSink::HasLiveSnapshot() const
{
	return Snapshot.ContainsByPredicate([](const FSnap& S) { return S.Actor.IsValid(); });
}

void FXEditorSink::ActiveBasis(FVector& X, FVector& Y, FVector& Z) const
{
	// Return the basis snapshotted at op start (see CaptureSelection), not the live actor rotation.
	X = ActiveBX;
	Y = ActiveBY;
	Z = ActiveBZ;
}

void FXEditorSink::ClearComponent(EXMode Mode)
{
	if (!GEditor || !HasSelection())
	{
		return;
	}

	FScopedTransaction Tx(LOCTEXT("XformClear", "Blender Clear Transform"));

	bool bAny = false;
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!Actor)
		{
			continue;
		}
		Actor->Modify();
		if (USceneComponent* Root = Actor->GetRootComponent())
		{
			Root->Modify();
		}

		FTransform T = Actor->GetActorTransform();
		switch (Mode)
		{
			case EXMode::Move:   T.SetLocation(FVector::ZeroVector); break; // Alt+G
			case EXMode::Scale:  T.SetScale3D(FVector::OneVector);   break; // Alt+S
			case EXMode::Rotate: T.SetRotation(FQuat::Identity);     break; // Alt+R
			default: break;
		}
		Actor->SetActorTransform(T, false, nullptr, ETeleportType::TeleportPhysics);
		bAny = true;
	}

	if (!bAny)
	{
		Tx.Cancel();
		return;
	}

	if (GUnrealEd)
	{
		GUnrealEd->SetPivotMovedIndependently(false);
		GUnrealEd->UpdatePivotLocationForSelection();
	}
	GEditor->NoteSelectionChange();
	GEditor->RedrawLevelEditingViewports();
}

#undef LOCTEXT_NAMESPACE
