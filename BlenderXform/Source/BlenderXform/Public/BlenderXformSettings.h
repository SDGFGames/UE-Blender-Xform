#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BlenderXformSettings.generated.h"

/** Which bounds surface snap measures contact from (Task 3.2 / D4). Default Collision matches the
 *  pre-3.2 behavior. */
UENUM()
enum class EBlenderXformSurfaceBoundsPolicy : uint8
{
	Collision /**/ UMETA(DisplayName = "Collision Bounds"),
	Visual    /**/ UMETA(DisplayName = "Visual Bounds"),
	Origin    /**/ UMETA(DisplayName = "Actor Origin"),
};

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

	/** Surface snap contact reference (Task 3.2). Collision = pre-3.2 behavior; Visual = primitive
	 *  bounds fallback; Origin = the actor's pivot point. */
	UPROPERTY(EditAnywhere, config, Category = "Surface Snap")
	EBlenderXformSurfaceBoundsPolicy SurfaceBoundsPolicy = EBlenderXformSurfaceBoundsPolicy::Collision;

	/** Safety gap kept between the bounds and the surface (Task 3.2). Was a fixed 0.1 cm before. */
	UPROPERTY(EditAnywhere, config, Category = "Surface Snap", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float SurfaceGap = 0.1f;

	// Snap sizes are intentionally NOT settings here — they are bound to UE's viewport grid (Snap
	// Settings dropdowns for translate/rotate/scale). Snapping engages when the matching UE grid-snap
	// toggle is on, or when Ctrl is held (Blender on-demand snap).

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};
