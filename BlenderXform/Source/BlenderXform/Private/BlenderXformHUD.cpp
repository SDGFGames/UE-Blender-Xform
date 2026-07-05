#include "BlenderXformHUD.h"

#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"            // UFont::GetStringSize / GetMaxCharHeight
#include "CanvasItem.h"
#include "SceneTypes.h"             // SE_BLEND_Translucent (FCanvasTileItem blend)
#include "LevelEditorViewport.h"    // GCurrentLevelEditingViewportClient (cursor pixel; same source as the input processor)
#include "UnrealClient.h"           // FViewport::GetMousePos

FBlenderXformHUD::FBlenderXformHUD(TFunction<FString()> InGetText, TFunction<FXAxisOverlay()> InGetAxes)
	: GetText(MoveTemp(InGetText))
	, GetAxes(MoveTemp(InGetAxes))
{
}

FString FBlenderXformHUD::HintTextForViewportWidth(float ViewportWidth)
{
	static const FString FullHints = TEXT("[X/Y/Z] axis   [Shift+axis] plane   [Shift] fine   [Ctrl] snap   [0-9 . -] type   [Enter/LMB] confirm   [Esc/RMB] cancel");
	static const FString CompactHints = TEXT("[X/Y/Z] axis   [Shift] fine   [Ctrl] snap   [Enter/LMB] confirm   [Esc/RMB] cancel");
	return ViewportWidth < 800.0f ? CompactHints : FullHints;
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

	// The compact cursor tag; empty when no op is active => draw neither the tag nor the hint bar.
	const FString Tag = GetText();
	if (Tag.IsEmpty())
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const FLinearColor PanelColor(0.0f, 0.0f, 0.0f, 0.5f); // translucent backing for legibility
	const FVector2D Pad(6.0f, 3.0f);

	// --- value tag riding next to the cursor (Blender's mouse read-out) ---
	// Cursor in viewport pixels from the active level viewport (same source the input processor uses);
	// fall back to a fixed corner when it's unavailable.
	FVector2D Cursor(28.0f, 64.0f);
	if (FLevelEditorViewportClient* VC = GCurrentLevelEditingViewportClient)
	{
		if (VC->Viewport)
		{
			FIntPoint MP;
			VC->Viewport->GetMousePos(MP);
			if (MP.X >= 0 && MP.Y >= 0)
			{
				Cursor = FVector2D(MP.X, MP.Y);
			}
		}
	}

	const float TagW = Font->GetStringSize(*Tag);
	const float TagH = Font->GetMaxCharHeight();

	// Offset from the cursor, then clamp so the whole panel stays inside the viewport.
	FVector2D TagPos = Cursor + FVector2D(18.0f, 8.0f);
	TagPos.X = FMath::Clamp(TagPos.X, 2.0f, FMath::Max(2.0f, Canvas->SizeX - TagW - Pad.X * 2.0f - 2.0f));
	TagPos.Y = FMath::Clamp(TagPos.Y, 2.0f, FMath::Max(2.0f, Canvas->SizeY - TagH - Pad.Y * 2.0f - 2.0f));

	{
		FCanvasTileItem Panel(TagPos - Pad, FVector2D(TagW + Pad.X * 2.0f, TagH + Pad.Y * 2.0f), PanelColor);
		Panel.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Panel);
	}
	FCanvasTextItem TagItem(TagPos, FText::FromString(Tag), Font, FLinearColor(1.0f, 0.85f, 0.1f));
	TagItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(TagItem);

	// --- static key-hint bar pinned to the bottom of the viewport ---
	const FString Hints = HintTextForViewportWidth(Canvas->SizeX);
	UFont* HintFont = GEngine ? GEngine->GetSmallFont() : Font;
	if (HintFont)
	{
		const float BarH = HintFont->GetMaxCharHeight() + Pad.Y * 2.0f;
		const float BarY = Canvas->SizeY - BarH - 6.0f;

		FCanvasTileItem Bar(FVector2D(0.0f, BarY), FVector2D(Canvas->SizeX, BarH), PanelColor);
		Bar.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Bar);

		FCanvasTextItem HintItem(FVector2D(12.0f, BarY + Pad.Y), FText::FromString(Hints), HintFont, FLinearColor(0.9f, 0.9f, 0.9f));
		HintItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(HintItem);
	}
}
