using UnrealBuildTool;

public class BlenderXform : ModuleRules
{
	public BlenderXform(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",             // FSlateApplication, IInputProcessor
			"SlateCore",
			"ApplicationCore",   // FModifierKeysState, key/pointer events
			"InputCore",         // EKeys
			"UnrealEd",          // GEditor, FScopedTransaction, selection, GCurrentLevelEditingViewportClient
			"LevelEditor",       // level viewport access
			"ToolMenus",         // toolbar toggle button
			"DeveloperSettings", // UBlenderXformSettings
		});

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// CGEventSourceKeyState — poll the hardware Escape key, which macOS can route away from
			// Slate input pre-processors before the plugin sees it.
			PublicFrameworks.Add("CoreGraphics");
		}
	}
}
