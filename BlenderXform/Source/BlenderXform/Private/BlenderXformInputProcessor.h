#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "InputCoreTypes.h"
#include "BlenderXformOp.h"
#include "BlenderXformEditorSink.h"

struct FKeyEvent;
struct FPointerEvent;
class FSlateApplication;
class FLevelEditorViewportClient;

struct FBlenderXformInputPolicy
{
	static bool ShouldConsumeUnsafeEditorShortcut(const FKey& Key, bool bControlDown, bool bCommandDown);
};

/**
 * Slate input pre-processor implementing Blender-style modal G/S/R in the level viewport.
 * Runs before normal Slate routing, so consuming an event (returning true) shadows UE's default
 * binding (e.g. G/R). It only engages when the settings toggle is on, the cursor is over the
 * active level viewport, and (to start) something is selected — otherwise it passes everything
 * through, leaving native UE untouched. While an op is active it owns keyboard/mouse until the
 * user commits (LMB/Enter) or cancels (RMB/Esc).
 */
class FBlenderXformInputProcessor : public IInputProcessor
{
public:
	virtual ~FBlenderXformInputProcessor() override = default;

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("BlenderXform"); }

	const FBlenderXformOp& GetOp() const { return Op; }

private:
	bool IsEnabled() const;
	bool CanStartHere() const;     // enabled + cursor over the active level viewport + has selection
	void CaptureStartCursor();     // record the pixel where the op began
	void UpdateFromMouse();        // refresh tuning + deproject current cursor + feed the op
	FXTuning BuildTuning(bool bSnap, bool bPrecision) const; // settings + live Ctrl(snap)/Shift(fine)
	void RefreshTuningFromModifiers(); // read current Ctrl/Shift and push to the op (no key interception)
	bool ComputeSnapActive() const;    // Ctrl held OR UE's per-mode grid-snap toggle on (for the op's mode)
	bool EnsureViewCache(FLevelEditorViewportClient* VC); // (re)compute the scene view at most once per frame
	bool DeprojectToPlane(const FVector2D& Pixel, const FVector& PlanePt, const FVector& PlaneN, FVector& OutWorld) const;

	FBlenderXformOp Op;
	FXEditorSink Sink;
	FVector2D StartPixel = FVector2D::ZeroVector;
	bool bLastSnapActive = false;  // last snap state (Ctrl || UE grid-snap toggle) + Shift, to re-apply
	bool bLastShiftDown = false;   // on a stationary change (tap Ctrl, or flip the UE snap toggle)
	FXTuning CachedBaseTuning;      // settings read once per op (modifiers patched in per move) — avoids per-move GetDefault

	// Per-frame scene-view cache: CalcSceneView is rebuilt at most once per frame (and when the active
	// viewport changes), so multiple mouse events in one frame reuse it. Keyed on GFrameCounter so it
	// self-invalidates when the camera moves next frame.
	uint64 CachedViewFrame = (uint64)-1;
	FLevelEditorViewportClient* CachedViewVC = nullptr;
	FIntRect CachedViewRect;
	FMatrix CachedViewProj = FMatrix::Identity;
	FMatrix CachedInvViewProj = FMatrix::Identity;
	FVector CachedCamFwd = FVector::ForwardVector;
	bool bViewCacheValid = false;
};
