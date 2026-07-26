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

/**
 * Keyboard-focus context for a potential/active op, gathered from Slate. Kept as a plain struct so the
 * start/deny policy below is unit-testable without a live Slate application (Task 1.2).
 *  - bViewportKeyboardFocus: the keyboard focus is on (or inside) the initiating level viewport widget.
 *  - bTextInputFocus:        the keyboard focus is on an editable-text widget (rename, details, search, save dialog).
 *  - bModalWindowActive:     a Slate modal window is open (and would swallow/redirect keys).
 */
struct FBlenderXformInputContext
{
	bool bViewportKeyboardFocus = false;
	bool bTextInputFocus = false;
	bool bModalWindowActive = false;
};

struct FBlenderXformInputPolicy
{
	static bool ShouldToggleSurfaceSnap(EXMode Mode, const FKey& Key, bool bIsRepeat,
	                                    bool bShiftDown, bool bControlDown, bool bCommandDown, bool bAltDown);
	static bool HasMovedForSurfaceSnap(const FVector2D& Start, const FVector2D& Now);
	static bool OwnsViewport(const FLevelEditorViewportClient* Initiating, const FLevelEditorViewportClient* Candidate);
	/** A G/S/R op may start only when focus is in the viewport and not in a text field or modal window. */
	static bool CanStartInContext(const FBlenderXformInputContext& C);
	/**
	 * During an active op, should a modifier chord (Ctrl/Cmd/Alt held) be passed through to the editor?
	 * Default-deny (D3): only the plugin's own toggle hotkey Alt+Shift+B is allowed through - it turns
	 * the plugin off, and the next Tick cancels the op. Everything else is consumed so it can't mutate
	 * editor state (Duplicate/Cut/Paste/Select All/Save/...) while our transaction is open.
	 */
	static bool ShouldPassThroughModifierChord(const FKey& Key, bool bShiftDown, bool bControlDown,
	                                            bool bCommandDown, bool bAltDown);
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
	bool CanStartHere() const;     // enabled + cursor over the active level viewport + viewport keyboard focus + has selection
	bool IsKeyboardFocusInInitiatingViewport() const; // keyboard focus is on/inside the op's bound viewport widget
	FBlenderXformInputContext BuildInputContext() const; // gather Slate focus/modal state into the testable struct
	void CaptureStartCursor();     // record the pixel + binding viewport where the op began
	void UpdateFromMouse();        // refresh tuning + deproject current cursor + feed the op
	void EndActiveOperation(bool bCommit); // single lifecycle exit: Commit/Cancel + release the bound viewport
	bool OwnsViewport(const FLevelEditorViewportClient* VC) const; // true if VC is the viewport the op started in
	bool IsInitiatingViewportLive() const; // bound viewport still registered (not closed/destroyed)
	FXTuning BuildTuning(bool bSnap, bool bPrecision) const; // settings + live Ctrl(snap)/Shift(fine)
	void RefreshTuningFromModifiers(); // read current Ctrl/Shift and push to the op (no key interception)
	bool ComputeSnapActive() const;    // Ctrl held OR UE's per-mode grid-snap toggle on (for the op's mode)
	bool EnsureViewCache(FLevelEditorViewportClient* VC); // (re)compute the scene view at most once per frame
	bool DeprojectRay(const FVector2D& Pixel, FVector& OutOrigin, FVector& OutDirection) const;
	bool DeprojectToPlane(const FVector2D& Pixel, const FVector& PlanePt, const FVector& PlaneN, FVector& OutWorld) const;

	FBlenderXformOp Op;
	FXEditorSink Sink;
	FLevelEditorViewportClient* InitiatingViewportClient = nullptr; // viewport the active op started in; null when idle
	FVector2D StartPixel = FVector2D::ZeroVector;
	bool bSurfaceSnapEnabled = false;      // session-level C toggle; persists across Commit/Cancel
	bool bMouseMovedSinceOpStart = false; // prevents an enabled mode from teleporting on the G key-down
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
