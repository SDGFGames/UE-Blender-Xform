#pragma once

#include "CoreMinimal.h"

class UCanvas;
class APlayerController;

/**
 * Constraint guide lines to overlay during an op: a world-space line through Pivot along each Dir,
 * drawn in the matching Color (Blender's red/green/blue axis lines). Empty Dirs => nothing to draw.
 */
struct FXAxisOverlay
{
	bool bActive = false;
	FVector Pivot = FVector::ZeroVector;
	TArray<FVector> Dirs;          // world unit directions (1 for a single axis, 2 for a plane)
	TArray<FLinearColor> Colors;   // one per Dir
};

/**
 * Draws the active op's status string (e.g. "Move | X (local) | 5") plus axis guide lines onto the
 * editor viewport via UDebugDrawService. Decoupled from the op: it pulls the text and the overlay
 * through supplied callbacks, which return empty/inactive when no op is running (so nothing draws).
 */
class FBlenderXformHUD
{
public:
	FBlenderXformHUD(TFunction<FString()> InGetText, TFunction<FXAxisOverlay()> InGetAxes);

	static FString HintTextForViewportWidth(float ViewportWidth);

	void Register();
	void Unregister();

private:
	void Draw(UCanvas* Canvas, APlayerController* PC);

	TFunction<FString()> GetText;
	TFunction<FXAxisOverlay()> GetAxes;
	FDelegateHandle Handle;
};
