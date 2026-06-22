#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BlenderXformSettings.generated.h"

/**
 * Editor settings for Blender Transform Shortcuts. bEnabled is THE toggle: when false the input
 * pre-processor passes every event through and UE behaves exactly as stock. Reachable under
 * Editor Preferences > Plugins > Blender Transform Shortcuts, and flipped by the toolbar button.
 */
UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "Blender Transform Shortcuts"))
class UBlenderXformSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Master on/off. When off, native UE shortcuts (G = Game View, R = Scale gizmo, ...) are untouched. */
	UPROPERTY(EditAnywhere, config, Category = "Blender Transform")
	bool bEnabled = true;

	/** Scales mouse-driven translate/scale magnitude. */
	UPROPERTY(EditAnywhere, config, Category = "Feel", meta = (ClampMin = "0.05", ClampMax = "10.0"))
	float MouseSensitivity = 1.0f;

	/** Scales mouse-driven rotation magnitude. */
	UPROPERTY(EditAnywhere, config, Category = "Feel", meta = (ClampMin = "0.05", ClampMax = "10.0"))
	float RotateSensitivity = 1.0f;

	/** Hold Shift during a drag for this fine-control multiplier (Blender precision mode). */
	UPROPERTY(EditAnywhere, config, Category = "Feel", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float PrecisionScale = 0.1f;

	/** Hold Ctrl during a Move drag to snap translation to this grid (world units / cm). */
	UPROPERTY(EditAnywhere, config, Category = "Snapping", meta = (ClampMin = "0.001"))
	float MoveSnapInterval = 10.0f;

	/** Hold Ctrl during a Rotate drag to snap the angle to this increment (degrees). */
	UPROPERTY(EditAnywhere, config, Category = "Snapping", meta = (ClampMin = "0.1", ClampMax = "180.0"))
	float RotateSnapInterval = 5.0f;

	/** Hold Ctrl during a Scale drag to snap the factor to this increment. */
	UPROPERTY(EditAnywhere, config, Category = "Snapping", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float ScaleSnapInterval = 0.1f;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};
