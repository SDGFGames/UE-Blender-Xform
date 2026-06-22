#include "BlenderXformStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateVectorImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

TSharedPtr<FSlateStyleSet> FBlenderXformStyle::StyleInstance = nullptr;

void FBlenderXformStyle::Initialize()
{
	if (StyleInstance.IsValid())
	{
		return;
	}

	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlenderXform")))
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	const FVector2D Icon20(20.0, 20.0);
	Style->Set("BlenderXform.ModeOn",
		new FSlateVectorImageBrush(Style->RootToContentDir(TEXT("BlenderModeOn"), TEXT(".svg")), Icon20));
	Style->Set("BlenderXform.ModeOff",
		new FSlateVectorImageBrush(Style->RootToContentDir(TEXT("BlenderModeOff"), TEXT(".svg")), Icon20));

	FSlateStyleRegistry::RegisterSlateStyle(*Style);
	StyleInstance = Style;
}

void FBlenderXformStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}
}

FName FBlenderXformStyle::GetStyleSetName()
{
	static const FName StyleSetName(TEXT("BlenderXformStyle"));
	return StyleSetName;
}

const ISlateStyle& FBlenderXformStyle::Get()
{
	check(StyleInstance.IsValid()); // must call Initialize() first (and before Shutdown())
	return *StyleInstance;
}
