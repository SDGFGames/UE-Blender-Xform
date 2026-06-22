#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Framework/Application/SlateApplication.h"

#include "BlenderXformInputProcessor.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlenderXform, Log, All);

/**
 * Editor module for Blender-style transform shortcuts. Registers the Slate input pre-processor that
 * implements the modal G/S/R behavior. (HUD draw + toolbar toggle are wired in later tasks.)
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
			UE_LOG(LogBlenderXform, Log, TEXT("[BlenderXform] input pre-processor registered"));
		}
	}

	virtual void ShutdownModule() override
	{
		if (FSlateApplication::IsInitialized() && InputProcessor.IsValid())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		}
		InputProcessor.Reset();
		UE_LOG(LogBlenderXform, Log, TEXT("[BlenderXform] shutdown"));
	}

private:
	TSharedPtr<FBlenderXformInputProcessor> InputProcessor;
};

IMPLEMENT_MODULE(FBlenderXformModule, BlenderXform)
