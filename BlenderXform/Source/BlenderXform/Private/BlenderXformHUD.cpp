#include "BlenderXformHUD.h"

#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"

FBlenderXformHUD::FBlenderXformHUD(TFunction<FString()> InGetText)
	: GetText(MoveTemp(InGetText))
{
}

void FBlenderXformHUD::Register()
{
	// "Editor" show flag is on in level-editor viewports, so this overlay draws there.
	Handle = UDebugDrawService::Register(TEXT("Editor"),
		FDebugDrawDelegate::CreateRaw(this, &FBlenderXformHUD::Draw));
}

void FBlenderXformHUD::Unregister()
{
	if (Handle.IsValid())
	{
		UDebugDrawService::Unregister(Handle);
		Handle.Reset();
	}
}

void FBlenderXformHUD::Draw(UCanvas* Canvas, APlayerController* /*PC*/)
{
	if (!Canvas || !GetText)
	{
		return;
	}

	const FString Text = GetText();
	if (Text.IsEmpty())
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font)
	{
		return;
	}

	FCanvasTextItem Item(FVector2D(28.0f, 64.0f), FText::FromString(Text), Font, FLinearColor(1.0f, 0.85f, 0.1f));
	Item.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Item);
}
