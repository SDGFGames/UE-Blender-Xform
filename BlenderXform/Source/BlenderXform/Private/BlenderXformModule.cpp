#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Framework/Application/SlateApplication.h"
#include "ToolMenus.h"
#include "Textures/SlateIcon.h"
#include "Styling/AppStyle.h"
#include "Framework/Commands/Commands.h"      // TCommands, UI_COMMAND
#include "Framework/Commands/UICommandList.h" // MapAction / UnmapAction
#include "Framework/Commands/InputChord.h"    // FInputChord, EModifierKey
#include "InputCoreTypes.h"                    // EKeys
#include "LevelEditor.h"                       // FLevelEditorModule (global command list)

#include "BlenderXformInputProcessor.h"
#include "BlenderXformHUD.h"
#include "BlenderXformSettings.h"
#include "BlenderXformStyle.h"

#define LOCTEXT_NAMESPACE "BlenderXform"

DEFINE_LOG_CATEGORY_STATIC(LogBlenderXform, Log, All);

namespace
{
	bool IsXformEnabled()
	{
		const UBlenderXformSettings* S = GetDefault<UBlenderXformSettings>();
		return S && S->bEnabled;
	}

	void ToggleXformEnabled()
	{
		if (UBlenderXformSettings* S = GetMutableDefault<UBlenderXformSettings>())
		{
			S->bEnabled = !S->bEnabled;
			S->SaveConfig();
		}
		// Re-poll the toolbar button so its icon swaps on/off immediately.
		if (UToolMenus* Menus = UToolMenus::Get())
		{
			Menus->RefreshAllWidgets();
		}
	}
}

/** Editor commands for the plugin — currently just the on/off toggle (default Alt+Shift+B). */
class FBlenderXformCommands : public TCommands<FBlenderXformCommands>
{
public:
	FBlenderXformCommands()
		: TCommands<FBlenderXformCommands>(
			TEXT("BlenderXform"),
			LOCTEXT("BlenderXformCommands", "Blender Transform"),
			NAME_None,
			FAppStyle::GetAppStyleSetName())
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(ToggleEnabled, "Toggle Blender Transform",
			"Enable/disable the Blender-style G/S/R transform shortcuts in the viewport",
			EUserInterfaceActionType::ToggleButton,
			FInputChord(EModifierKey::Alt | EModifierKey::Shift, EKeys::B));
	}

	TSharedPtr<FUICommandInfo> ToggleEnabled;
};

/**
 * Editor module for Blender-style transform shortcuts. Owns the Slate input pre-processor (the
 * modal G/S/R engine), the viewport HUD overlay, and a level-editor toolbar toggle bound to the
 * master on/off setting.
 */
class FBlenderXformModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FBlenderXformStyle::Initialize();

		if (FSlateApplication::IsInitialized())
		{
			FBlenderXformCommands::Register(); // toggle command (hotkey bound once the level editor is up)

			InputProcessor = MakeShared<FBlenderXformInputProcessor>();
			// Index 0 = first in line, so no other pre-processor (editor or plugin) can swallow our
			// keys first — notably Escape, which on macOS was being consumed before we saw it.
			FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);

			FBlenderXformInputProcessor* Proc = InputProcessor.Get();
			HUD = MakeUnique<FBlenderXformHUD>(
				[Proc]() -> FString
				{
					return (Proc && Proc->GetOp().IsActive()) ? Proc->GetOp().Hud() : FString();
				},
				[Proc]() -> FXAxisOverlay
				{
					FXAxisOverlay O;
					if (!Proc || !Proc->GetOp().IsActive())
					{
						return O;
					}
					const FBlenderXformOp& Op = Proc->GetOp();
					const FXConstraint& C = Op.Constraint();
					if (!C.HasConstraint())
					{
						return O; // Free: no constraint line (Blender shows none either)
					}

					auto AxisColor = [](EXAxis Ax) -> FLinearColor
					{
						switch (Ax)
						{
							case EXAxis::X: return FLinearColor(0.90f, 0.20f, 0.20f); // red
							case EXAxis::Y: return FLinearColor(0.40f, 0.80f, 0.20f); // green
							case EXAxis::Z: return FLinearColor(0.25f, 0.50f, 0.95f); // blue
							default:        return FLinearColor::White;
						}
					};

					O.bActive = true;
					O.Pivot = Op.LastApplied().Pivot;

					if (!C.bPlane)
					{
						O.Dirs.Add(C.AxisDir().GetSafeNormal());
						O.Colors.Add(AxisColor(C.Axis));
					}
					else
					{
						// plane: draw the two axes that remain free (the locked one is omitted)
						if (C.Axis != EXAxis::X) { O.Dirs.Add(C.BX.GetSafeNormal()); O.Colors.Add(AxisColor(EXAxis::X)); }
						if (C.Axis != EXAxis::Y) { O.Dirs.Add(C.BY.GetSafeNormal()); O.Colors.Add(AxisColor(EXAxis::Y)); }
						if (C.Axis != EXAxis::Z) { O.Dirs.Add(C.BZ.GetSafeNormal()); O.Colors.Add(AxisColor(EXAxis::Z)); }
					}
					return O;
				});
			HUD->Register();

			UE_LOG(LogBlenderXform, Log, TEXT("[BlenderXform] input pre-processor + HUD registered"));
		}

		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlenderXformModule::RegisterToolbar));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);

		if (HUD.IsValid())
		{
			HUD->Unregister();
			HUD.Reset();
		}

		if (FSlateApplication::IsInitialized() && InputProcessor.IsValid())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		}
		InputProcessor.Reset();

		if (FBlenderXformCommands::IsRegistered())
		{
			if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
			{
				FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
				LevelEditor.GetGlobalLevelEditorActions()->UnmapAction(FBlenderXformCommands::Get().ToggleEnabled);
			}
			FBlenderXformCommands::Unregister();
		}

		FBlenderXformStyle::Shutdown();

		UE_LOG(LogBlenderXform, Log, TEXT("[BlenderXform] shutdown"));
	}

private:
	void RegisterToolbar()
	{
		FToolMenuOwnerScoped OwnerScoped(this);

		UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
		if (!Toolbar)
		{
			return;
		}

		FToolMenuSection& Section = Toolbar->FindOrAddSection("BlenderXform");

		FToolUIAction Action;
		Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
			[](const FToolMenuContext&) { ToggleXformEnabled(); });
		Action.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda(
			[](const FToolMenuContext&) { return IsXformEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; });

		// Icon reflects on/off live: orange transform glyph when active, muted+slashed when off.
		TAttribute<FSlateIcon> DynamicIcon = TAttribute<FSlateIcon>::Create(
			TAttribute<FSlateIcon>::FGetter::CreateLambda([]()
			{
				return FSlateIcon(FBlenderXformStyle::GetStyleSetName(),
					IsXformEnabled() ? "BlenderXform.ModeOn" : "BlenderXform.ModeOff");
			}));

		FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
			"BlenderXformToggle",
			FToolUIActionChoice(Action),
			LOCTEXT("ToggleLabel", "Blender XForm"),
			LOCTEXT("ToggleTip", "Toggle Blender-style G/S/R transform shortcuts in the viewport"),
			DynamicIcon,
			EUserInterfaceActionType::ToggleButton);
		Entry.SetCommandList(nullptr);

		Section.AddEntry(Entry);

		// Bind the keyboard shortcut (default Alt+Shift+B; rebindable in Editor Preferences ->
		// Keyboard Shortcuts -> Blender Transform) into the level editor's global command list, which
		// the viewport processes — so the toggle works from the 3D view. Mapped here, where the level
		// editor is guaranteed loaded.
		if (FBlenderXformCommands::IsRegistered())
		{
			FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
			const TSharedRef<FUICommandList> Actions = LevelEditor.GetGlobalLevelEditorActions();
			if (!Actions->IsActionMapped(FBlenderXformCommands::Get().ToggleEnabled))
			{
				Actions->MapAction(
					FBlenderXformCommands::Get().ToggleEnabled,
					FExecuteAction::CreateStatic(&ToggleXformEnabled),
					FCanExecuteAction(),
					FIsActionChecked::CreateStatic(&IsXformEnabled));
			}
		}
	}

	TSharedPtr<FBlenderXformInputProcessor> InputProcessor;
	TUniquePtr<FBlenderXformHUD> HUD;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlenderXformModule, BlenderXform)
