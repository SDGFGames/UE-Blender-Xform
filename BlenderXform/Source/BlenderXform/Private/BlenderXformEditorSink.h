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
	virtual void Apply(const FXApplied& In) override;
	virtual void Commit() override;
	virtual void Cancel() override;
	virtual FVector Pivot() const override;
	virtual void ActiveBasis(FVector& X, FVector& Y, FVector& Z) const override;

	bool HasSelection() const;

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
};
