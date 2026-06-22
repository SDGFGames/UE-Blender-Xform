#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "BlenderXformOp.h"
#include "BlenderXformEditorSink.h"

struct FKeyEvent;
struct FPointerEvent;
class FSlateApplication;

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
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("BlenderXform"); }

	const FBlenderXformOp& GetOp() const { return Op; }

private:
	bool IsEnabled() const;
	bool CanStartHere() const;     // enabled + cursor over the active level viewport + has selection
	void CaptureStartCursor();     // record the pixel where the op began
	void UpdateFromMouse();        // deproject current cursor + feed the op

	FBlenderXformOp Op;
	FXEditorSink Sink;
	FVector2D StartPixel = FVector2D::ZeroVector;
};
