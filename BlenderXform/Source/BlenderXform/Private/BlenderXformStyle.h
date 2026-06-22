#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * Tiny Slate style set owning the two toolbar icons that show the mode's on/off state:
 *   "BlenderXform.ModeOn"  — Blender-orange transform glyph (mode active)
 *   "BlenderXform.ModeOff" — muted, slashed glyph (mode off)
 * Loaded from the plugin's Resources/ as SVG so it stays crisp at any DPI.
 */
class FBlenderXformStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static FName GetStyleSetName();
	static const ISlateStyle& Get();

private:
	static TSharedPtr<class FSlateStyleSet> StyleInstance;
};
