#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Framework/Application/SlateApplication.h"
#include "ToolMenus.h"
#include "Textures/SlateIcon.h"
#include "Styling/AppStyle.h"

#include "BlenderXformInputProcessor.h"
#include "BlenderXformHUD.h"
#include "BlenderXformSettings.h"

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
	}
}

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
		if (FSlateApplication::IsInitialized())
		{
			InputProcessor = MakeShared<FBlenderXformInputProcessor>();
			FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);

			FBlenderXformInputProcessor* Proc = InputProcessor.Get();
			HUD = MakeUnique<FBlenderXformHUD>([Proc]() -> FString
			{
				return (Proc && Proc->GetOp().IsActive()) ? Proc->GetOp().HudString() : FString();
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

		FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
			"BlenderXformToggle",
			FToolUIActionChoice(Action),
			LOCTEXT("ToggleLabel", "Blender XForm"),
			LOCTEXT("ToggleTip", "Toggle Blender-style G/S/R transform shortcuts in the viewport"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Transform"));
		Entry.SetCommandList(nullptr);

		Section.AddEntry(Entry);
	}

	TSharedPtr<FBlenderXformInputProcessor> InputProcessor;
	TUniquePtr<FBlenderXformHUD> HUD;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlenderXformModule, BlenderXform)
