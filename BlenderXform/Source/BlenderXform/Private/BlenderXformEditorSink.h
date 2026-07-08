#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "BlenderXformOp.h" // IXApplySink, FXApplied

class AActor;
class FScopedTransaction;

/**
 * Production IXApplySink: applies op previews to the current level-editor selection.
 * Begin() snapshots every selected actor's transform and opens one FScopedTransaction (so the
 * whole op is a single Undo step); Apply() recomputes each actor from its snapshot + the preview
 * (never accumulating); Cancel() restores the snapshots and discards the transaction.
 */
class FXEditorSink : public IXApplySink
{
public:
	virtual ~FXEditorSink();

	virtual void Begin() override;
	virtual void BeginDuplicate() override;
	virtual void Apply(const FXApplied& In) override;
	virtual void Commit() override;
	virtual void Cancel() override;
	virtual FVector Pivot() const override;
	virtual void ActiveBasis(FVector& X, FVector& Y, FVector& Z) const override;

	static bool CanTransformActorClass(const UClass* ActorClass);
	static bool CanTransformActor(const AActor* Actor);
	bool HasSelection() const;

	/**
	 * Re-assert the post-cancel selection for a few frames. UE's native Escape-deselect (SELECT NONE,
	 * bound to Escape on the level viewport) lands AFTER our hardware-Escape poll cancels the op, so an
	 * immediate re-select loses the race; this is driven from the input processor's Tick to win it.
	 * No-op when nothing is pending or the actors are already selected.
	 */
	void DrainReselect();

	/** Instantly clear one transform component on the selection (Blender Alt+G/S/R), as one undo step. */
	void ClearComponent(EXMode Mode);

	/** True while at least one snapshotted actor is still alive (false once they're all deleted mid-op). */
	bool HasLiveSnapshot() const;

private:
	struct FSnap
	{
		TWeakObjectPtr<AActor> Actor;
		FTransform Xform;
	};

	void CaptureSelection();

	TArray<FSnap> Snapshot;
	TUniquePtr<FScopedTransaction> Transaction;
	FVector CachedPivot = FVector::ZeroVector;

	// Active actor's local axes, snapshotted at op start so a Local constraint uses a fixed basis
	// instead of drifting with the live (mid-op) rotation.
	FVector ActiveBX = FVector::ForwardVector;
	FVector ActiveBY = FVector::RightVector;
	FVector ActiveBZ = FVector::UpVector;

	TArray<TWeakObjectPtr<AActor>> FullSelection;    // ALL selected actors at op start (pivot median + re-select)
	TArray<TWeakObjectPtr<AActor>> PendingReselect; // actors to keep selected after a cancel
	int32 ReselectTicksLeft = 0;                     // frames remaining to re-assert PendingReselect

	// Shift+D duplicate-grab state. bDuplicated routes Cancel() down the "remove the copy" path (the
	// user's chosen behavior); OriginalSelection is what we re-select when that happens.
	bool bDuplicated = false;
	TArray<TWeakObjectPtr<AActor>> OriginalSelection; // selection BEFORE the duplicate (cancel falls back to it)
};
