#include "BlenderXformHUD.h"

#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"

FBlenderXformHUD::FBlenderXformHUD(TFunction<FString()> InGetText, TFunction<FXAxisOverlay()> InGetAxes)
	: GetText(MoveTemp(InGetText))
	, GetAxes(MoveTemp(InGetAxes))
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
	if (!Canvas)
	{
		return;
	}

	// Constraint guide lines (Blender's red/green/blue axis lines through the pivot).
	if (GetAxes)
	{
		const FXAxisOverlay Axes = GetAxes();
		if (Axes.bActive)
		{
			const FVector PivotProj = Canvas->Project(Axes.Pivot, /*bClampToZeroPlane*/ false);
			if (PivotProj.Z > 0.0) // pivot in front of the camera
			{
				const FVector2D Pivot2D(PivotProj.X, PivotProj.Y);
				const double Span = Canvas->SizeX + Canvas->SizeY; // long enough to cross the viewport

				const int32 N = FMath::Min(Axes.Dirs.Num(), Axes.Colors.Num());
				for (int32 i = 0; i < N; ++i)
				{
					const FVector Near = Canvas->Project(Axes.Pivot + Axes.Dirs[i] * 50.0, false);
					if (Near.Z <= 0.0)
					{
						continue; // axis points behind the camera; skip rather than draw garbage
					}
					FVector2D ScreenDir = FVector2D(Near.X, Near.Y) - Pivot2D;
					if (ScreenDir.IsNearlyZero())
					{
						continue; // axis points straight at/away from the camera => no on-screen line
					}
					ScreenDir.Normalize();

					FCanvasLineItem Line(Pivot2D - ScreenDir * Span, Pivot2D + ScreenDir * Span);
					Line.SetColor(Axes.Colors[i]);
					Line.LineThickness = 1.5f;
					Canvas->DrawItem(Line);
				}
			}
		}
	}

	if (!GetText)
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
