#include "NightlightSanityWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// The bar is drawn at half the texture size, 1160x172, so it stays sharp at 1440p.
	const FVector2D BarSize(580.0f, 86.0f);
	const FMargin BarFillPadding(14.0f, 18.0f, 14.0f, 18.0f);
	const float EdgeWidth = 48.0f;
}

void UNightlightSanityBar::BuildTree()
{
	UVerticalBox* const Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WidgetTree->RootWidget = Root;

	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(Row);

	KidSun = WidgetTree->ConstructWidget<UNightlightBoilImage>(UNightlightBoilImage::StaticClass());
	KidSun->SetFrames(NightlightCrayonStyle::LoadBoilFrames(TEXT("Icons/Characters/T_Kid_Sun_Happy")), FVector2D(104.0f));
	if (UHorizontalBoxSlot* const SunSlot = Row->AddChildToHorizontalBox(KidSun))
	{
		SunSlot->SetVerticalAlignment(VAlign_Center);
		SunSlot->SetPadding(FMargin(0.0f, 0.0f, -18.0f, 0.0f));
	}

	USizeBox* const BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarBox->SetWidthOverride(BarSize.X);
	BarBox->SetHeightOverride(BarSize.Y);
	if (UHorizontalBoxSlot* const BarSlot = Row->AddChildToHorizontalBox(BarBox))
	{
		BarSlot->SetVerticalAlignment(VAlign_Center);
	}

	UOverlay* const Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	BarBox->AddChild(Layers);

	// Back to front: paper, ink background with the yellow fill, red edge, pencil frame.
	auto AddLayer = [this, Layers](UWidget* const Widget, const FMargin& LayerPadding, const EHorizontalAlignment HAlign)
	{
		if (UOverlaySlot* const LayerSlot = Layers->AddChildToOverlay(Widget))
		{
			LayerSlot->SetHorizontalAlignment(HAlign);
			LayerSlot->SetVerticalAlignment(VAlign_Fill);
			LayerSlot->SetPadding(LayerPadding);
		}
	};

	AddLayer(MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("SanityBar/T_SanityBar_Paper")), BarSize), FMargin(0.0f), HAlign_Fill);

	YellowFill = NightlightCrayonStyle::MakeTileBrush(NightlightCrayonStyle::LoadTexture(TEXT("SanityBar/T_Hatch_Yellow_Tile")), FVector2D(64.0f));
	GreyFill = NightlightCrayonStyle::MakeTileBrush(NightlightCrayonStyle::LoadTexture(TEXT("SanityBar/T_Hatch_Grey_Tile")), FVector2D(64.0f));

	// The ink is the bar's background, so the nightmare's side is whatever the fill has not reached
	// (Epic Games, Inc., 2026).
	BarStyle.SetBackgroundImage(NightlightCrayonStyle::MakeImageBrush(NightlightCrayonStyle::LoadTexture(TEXT("SanityBar/T_SanityBar_InkBack")), BarSize));
	BarStyle.SetFillImage(YellowFill);
	BarStyle.SetMarqueeImage(YellowFill);
	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	Bar->SetWidgetStyle(BarStyle);
	Bar->SetFillColorAndOpacity(FLinearColor::White);
	Bar->SetPercent(1.0f);
	Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddLayer(Bar, BarFillPadding, HAlign_Fill);

	RedEdge = MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("SanityBar/T_SanityBar_EdgeRed")), FVector2D(EdgeWidth, BarSize.Y));
	AddLayer(RedEdge, FMargin(0.0f), HAlign_Left);

	AddLayer(MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("SanityBar/T_SanityBar_Frame")), BarSize), FMargin(0.0f), HAlign_Fill);

	Beast = WidgetTree->ConstructWidget<UNightlightBoilImage>(UNightlightBoilImage::StaticClass());
	Beast->SetFrames(NightlightCrayonStyle::LoadBoilFrames(TEXT("Icons/Characters/T_Nightmare_Beast")), FVector2D(104.0f));
	if (UHorizontalBoxSlot* const BeastSlot = Row->AddChildToHorizontalBox(Beast))
	{
		BeastSlot->SetVerticalAlignment(VAlign_Center);
		BeastSlot->SetPadding(FMargin(-18.0f, 0.0f, 0.0f, 0.0f));
	}

	Label = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Kid, 22, FLinearColor::White);
	Label->SetShadowOffset(FVector2D(1.0f, 2.0f));
	Label->SetShadowColorAndOpacity(NightlightCrayonStyle::Ink());
	if (UVerticalBoxSlot* const LabelSlot = Root->AddChildToVerticalBox(Label))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetPadding(FMargin(0.0f, -10.0f, 0.0f, 0.0f));
	}

	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UNightlightSanityBar::SetSanity(const float Percent, const ENightlightSanityState State)
{
	EnsureBuilt();
	const float ClampedPercent = FMath::Clamp(Percent, 0.0f, 1.0f);
	Bar->SetPercent(ClampedPercent);

	// The red edge rides the end of the fill, where the sun and the ink meet.
	const float FillWidth = BarSize.X - BarFillPadding.Left - BarFillPadding.Right;
	if (UOverlaySlot* const EdgeSlot = Cast<UOverlaySlot>(RedEdge->Slot))
	{
		EdgeSlot->SetPadding(FMargin(BarFillPadding.Left + ClampedPercent * FillWidth - EdgeWidth * 0.5f, 0.0f, 0.0f, 0.0f));
	}

	if (ShownState.IsSet() && ShownState.GetValue() == State)
	{
		return;
	}

	ShownState = State;
	const bool bBrave = State == ENightlightSanityState::Brave;
	KidSun->SetFrames(
		NightlightCrayonStyle::LoadBoilFrames(bBrave ? TEXT("Icons/Characters/T_Kid_Sun_Happy") : TEXT("Icons/Characters/T_Kid_Sun_Scared")),
		FVector2D(104.0f));

	BarStyle.SetFillImage(State == ENightlightSanityState::LosingIt ? GreyFill : YellowFill);
	Bar->SetWidgetStyle(BarStyle);

	// The beast grows as it wins the tug of war.
	Beast->SetRenderScale(FVector2D(bBrave ? 0.9f : (State == ENightlightSanityState::Scared ? 1.05f : 1.25f)));

	switch (State)
	{
	case ENightlightSanityState::Brave:
		Label->SetText(NSLOCTEXT("NightlightCrayon", "SanityBrave", "i'm not scared!"));
		Label->SetFont(NightlightCrayonStyle::GetFont(ENightlightCrayonFont::Kid, 22));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		break;
	case ENightlightSanityState::Scared:
		Label->SetText(NSLOCTEXT("NightlightCrayon", "SanityScared", "um... it's getting dark"));
		Label->SetFont(NightlightCrayonStyle::GetFont(ENightlightCrayonFont::Kid, 22));
		Label->SetColorAndOpacity(FSlateColor(NightlightCrayonStyle::Sun()));
		break;
	default:
		Label->SetText(NSLOCTEXT("NightlightCrayon", "SanityLosing", "MAKE IT STOP"));
		Label->SetFont(NightlightCrayonStyle::GetFont(ENightlightCrayonFont::Nightmare, 24));
		Label->SetColorAndOpacity(FSlateColor(NightlightCrayonStyle::NightmareRed()));
		break;
	}
}

void UNightlightInkOverlay::BuildTree()
{
	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);

	auto AddFullScreen = [Root](UWidget* const Widget)
	{
		if (UOverlaySlot* const FullSlot = Root->AddChildToOverlay(Widget))
		{
			FullSlot->SetHorizontalAlignment(HAlign_Fill);
			FullSlot->SetVerticalAlignment(VAlign_Fill);
		}
	};

	if (UMaterialInterface* const Material = NightlightCrayonStyle::LoadMaterial(TEXT("M_UI_InkOverlay")))
	{
		InkMaterial = UMaterialInstanceDynamic::Create(Material, this);
		UImage* const InkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		InkImage->SetBrushFromMaterial(InkMaterial);
		InkImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddFullScreen(InkImage);
	}
	else
	{
		for (const TCHAR* const Stage : { TEXT("Brave"), TEXT("Scared"), TEXT("Losing"), TEXT("Overrun") })
		{
			UImage* const Vignette = MakeImage(
				NightlightCrayonStyle::LoadTexture(FString::Printf(TEXT("Ink/T_Ink_Vignette_%s"), Stage)),
				FVector2D(1920.0f, 1080.0f));
			Vignette->SetRenderOpacity(0.0f);
			AddFullScreen(Vignette);
			Vignettes.Add(Vignette);
		}
	}

	UCanvasPanel* const EyeLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	AddFullScreen(EyeLayer);

	// Eyes sit in the ink near the edges, never over the centre of the map.
	const FVector2D EyeAnchors[] = { FVector2D(0.06f, 0.30f), FVector2D(0.90f, 0.62f), FVector2D(0.14f, 0.84f) };
	for (const FVector2D& Anchor : EyeAnchors)
	{
		UImage* const Eye = MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("Ink/T_Nightmare_Eyes")), FVector2D(128.0f, 64.0f));
		Eye->SetRenderOpacity(0.0f);
		if (UCanvasPanelSlot* const EyeSlot = EyeLayer->AddChildToCanvas(Eye))
		{
			EyeSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
			EyeSlot->SetAlignment(FVector2D(0.5f));
			EyeSlot->SetAutoSize(true);
		}
		Eyes.Add(Eye);
	}
}

void UNightlightInkOverlay::UpdateInk(const float Percent, const ENightlightSanityState State, const float DeltaTime)
{
	EnsureBuilt();
	if (InkMaterial)
	{
		InkMaterial->SetScalarParameterValue(TEXT("Sanity"), Percent);
	}
	else
	{
		// Each vignette is fully shown inside its own band and fades into the next one.
		const float Stage = Percent >= 0.6f ? 0.0f
			: Percent >= 0.3f ? FMath::GetMappedRangeValueClamped(FVector2D(0.6f, 0.3f), FVector2D(0.0f, 1.0f), Percent)
			: Percent >= 0.15f ? 1.0f + FMath::GetMappedRangeValueClamped(FVector2D(0.3f, 0.15f), FVector2D(0.0f, 1.0f), Percent)
			: 2.0f + FMath::GetMappedRangeValueClamped(FVector2D(0.15f, 0.0f), FVector2D(0.0f, 1.0f), Percent);
		for (int32 Index = 0; Index < Vignettes.Num(); ++Index)
		{
			Vignettes[Index]->SetRenderOpacity(FMath::Clamp(1.0f - FMath::Abs(Stage - Index), 0.0f, 1.0f));
		}
	}

	EyeClock += DeltaTime;
	const float TargetOpacity = State == ENightlightSanityState::LosingIt ? 1.0f : 0.0f;
	EyeOpacity = FMath::FInterpTo(EyeOpacity, TargetOpacity, DeltaTime, 1.5f);
	for (int32 Index = 0; Index < Eyes.Num(); ++Index)
	{
		// Each pair blinks on its own slow cycle, so they never open and close together.
		const float Blink = FMath::Sin(EyeClock * (0.9f + Index * 0.37f) + Index * 2.1f) > -0.85f ? 1.0f : 0.0f;
		Eyes[Index]->SetRenderOpacity(EyeOpacity * Blink);
	}
}

/*
References

Epic Games, Inc., 2026. UProgressBar. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UProgressBar>
[Accessed 3 November 2026].
*/
