#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlenderXform, Log, All);

/**
 * Editor module for Blender-style transform shortcuts. Task 0 scaffold: logs lifecycle only.
 * Later tasks register the Slate input pre-processor, the HUD draw delegate, settings, and the
 * toolbar toggle here.
 */
class FBlenderXformModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogBlenderXform, Log, TEXT("[BlenderXform] StartupModule"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogBlenderXform, Log, TEXT("[BlenderXform] ShutdownModule"));
	}
};

IMPLEMENT_MODULE(FBlenderXformModule, BlenderXform)
