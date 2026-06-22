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
	}
}
