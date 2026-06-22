#pragma once

#include "CoreMinimal.h"
#include "BlenderXformTypes.h"

/** The preview a modal op produces each update; a sink applies it to the real selection. */
struct FXApplied
{
	EXMode  Mode      = EXMode::None;
	FVector MoveDelta = FVector::ZeroVector; // Move: world translation from the snapshot
	FVector ScaleFac  = FVector::OneVector;  // Scale: per-axis multipliers (for the HUD read-out)
	FMatrix ScaleMat  = FMatrix::Identity;   // Scale: world-space scale matrix the sink applies
	double  RotDeg    = 0.0;                  // Rotate: degrees about RotAxis
	FVector RotAxis   = FVector::ZeroVector;  // Rotate: world axis (Free => camera forward)
	FVector Pivot     = FVector::ZeroVector;  // Scale/Rotate pivot (world)
};

/**
 * The op touches the world (selection, transactions, deprojection) ONLY through this interface,
 * so the state machine is unit-testable with a fake sink.
 */
class IXApplySink
{
public:
	virtual ~IXApplySink() {}
	virtual void Begin() = 0;                  // snapshot selection + open the undo transaction
	virtual void Apply(const FXApplied& A) = 0; // apply a preview onto the snapshot
	virtual void Commit() = 0;                  // keep the transaction
	virtual void Cancel() = 0;                  // restore snapshot + cancel the transaction
	virtual FVector Pivot() const = 0;          // selection median (world)
	virtual void ActiveBasis(FVector& X, FVector& Y, FVector& Z) const = 0; // active actor local axes
};

/**
 * Blender-style modal transform state machine. Begin() snapshots via the sink; axis/numeric/mouse
 * changes re-apply a fresh preview computed from the snapshot (never accumulated); Commit/Cancel
 * end it. Pure math lives in FBlenderXformMath; this class only sequences input into math + sink.
 */
class FBlenderXformOp
{
public:
	void Begin(EXMode InMode, IXApplySink& Sink); // start, or switch mode mid-op (keeps the snapshot)
	void CycleAxis(EXAxis Axis, bool bShiftPlane); // same axis re-press: Global -> Local -> Free
	void PushDigit(TCHAR Ch);                      // 0-9, '.', '-' (toggles sign)
	void Backspace();
	void SetTuning(const FXTuning& InTuning);      // live feel knobs (sensitivity/precision/snap); re-applies
	void UpdateFromScreen(const FVector& WorldStart, const FVector& WorldNow,
	                      const FVector2D& PivotS, const FVector2D& StartS, const FVector2D& NowS,
	                      const FVector& CamForward);
	void Commit();
	void Cancel();

	bool IsActive() const { return Mode != EXMode::None; }
	EXMode GetMode() const { return Mode; }
	const FXConstraint& Constraint() const { return Con; }
	bool HasNumeric() const { return bHasNumeric; }
	double NumericValue() const { return NumVal; }
	const FXApplied& LastApplied() const { return Applied; }
	FString HudString() const;

private:
	void Reset();
	void ParseNumeric();
	void SetWorldBasis();
	void SetLocalBasis();
	void Recompute();
	static double LargestScaleComponent(const FVector& F); // the component that departs most from 1



	EXMode Mode = EXMode::None;
	FXConstraint Con;
	IXApplySink* Sink = nullptr;
	FXTuning Tuning;        // live feel knobs fed by the input processor
	FXApplied Applied;      // last preview pushed to the sink (for the HUD live read-out)

	FString NumBuf;
	bool bHasNumeric = false;
	double NumVal = 0.0;

	FVector LastWorldStart = FVector::ZeroVector;
	FVector LastWorldNow   = FVector::ZeroVector;
	FVector2D LastPivotS = FVector2D::ZeroVector;
	FVector2D LastStartS = FVector2D::ZeroVector;
	FVector2D LastNowS   = FVector2D::ZeroVector;
	FVector LastCamFwd = FVector(0, 0, -1);
};
