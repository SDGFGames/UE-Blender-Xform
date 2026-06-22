#pragma once

#include "CoreMinimal.h"

class UCanvas;
class APlayerController;

/**
 * Draws the active op's status string (e.g. "Move | X (local) | 5") onto the editor viewport via
 * UDebugDrawService. Decoupled from the op: it pulls the text through a supplied callback, which
 * returns empty when no op is active (so nothing draws).
 */
class FBlenderXformHUD
{
public:
	explicit FBlenderXformHUD(TFunction<FString()> InGetText);

	void Register();
	void Unregister();

private:
	void Draw(UCanvas* Canvas, APlayerController* PC);

	TFunction<FString()> GetText;
	FDelegateHandle Handle;
};
