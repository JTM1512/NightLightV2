#include "NightlightCrayonHealthBar.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

namespace
{
	// The sanity bar's pieces at about a tenth of their size, so the world bars match the HUD. The whole
	// widget fits the 145x48 draw size the defender and enemy widget components already use.
	const FVector2D WidgetSize(145.0f, 48.0f);
	const FVector2D StripSize(124.0f, 22.0f);
	const FMargin FillPadding(4.0f, 4.0f, 4.0f, 4.0f);
	const float DamageSeconds = 1.0f;
}

void UNightlightCrayonHealthBar::BuildTree()
{
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WidgetTree->RootWidget = SizeBox;
	SizeBox->SetWidthOverride(WidgetSize.X);
	SizeBox->SetHeightOverride(WidgetSize.Y);
	SizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);

	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SizeBox->AddChild(Root);

	USizeBox* const StripBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	StripBox->SetWidthOverride(StripSize.X);
	StripBox->SetHeightOverride(StripSize.Y);
	if (UOverlaySlot* const StripSlot = Root->AddChildToOverlay(StripBox))
	{
		StripSlot->SetHorizontalAlignment(HAlign_Center);
		StripSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	UOverlay* const Strip = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	StripBox->AddChild(Strip);
	auto AddLayer = [Strip](UWidget* const Widget, const FMargin& LayerPadding)
	{
		if (UOverlaySlot* const LayerSlot = Strip->AddChildToOverlay(Widget))
		{
			LayerSlot->SetHorizontalAlignment(HAlign_Fill);
			LayerSlot->SetVerticalAlignment(VAlign_Fill);
			LayerSlot->SetPadding(LayerPadding);
		}
	};

	AddLayer(MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("SanityBar/T_SanityBar_Paper")), StripSize), FMargin(0.0f));

	// The names match WBP_WorldHealthBar, so GetWidgetFromName finds them (Epic Games, Inc., 2026).
	FProgressBarStyle BarStyle;
	BarStyle.SetBackgroundImage(NightlightCrayonStyle::MakeImageBrush(NightlightCrayonStyle::LoadTexture(TEXT("SanityBar/T_SanityBar_InkBack")), StripSize));
	const FSlateBrush Hatch = NightlightCrayonStyle::MakeTileBrush(NightlightCrayonStyle::LoadTexture(TEXT("SanityBar/T_Hatch_Yellow_Tile")), FVector2D(20.0f));
	BarStyle.SetFillImage(Hatch);
	BarStyle.SetMarqueeImage(Hatch);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	HealthBar->SetWidgetStyle(BarStyle);
	HealthBar->SetPercent(1.0f);
	AddLayer(HealthBar, FillPadding);

	AddLayer(MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("SanityBar/T_SanityBar_Frame")), StripSize), FMargin(0.0f));

	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
	HealthText->SetFont(NightlightCrayonStyle::GetFont(ENightlightCrayonFont::Kid, 13));
	HealthText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	HealthText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	HealthText->SetShadowColorAndOpacity(NightlightCrayonStyle::Ink());
	HealthText->SetJustification(ETextJustify::Center);
	if (UOverlaySlot* const TextSlot = Strip->AddChildToOverlay(HealthText))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}

	DamageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DamageText"));
	DamageText->SetFont(NightlightCrayonStyle::GetFont(ENightlightCrayonFont::Nightmare, 16));
	DamageText->SetColorAndOpacity(FSlateColor(NightlightCrayonStyle::NightmareRed()));
	DamageText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	DamageText->SetShadowColorAndOpacity(NightlightCrayonStyle::Ink());
	DamageText->SetVisibility(ESlateVisibility::Hidden);
	if (UOverlaySlot* const DamageSlot = Root->AddChildToOverlay(DamageText))
	{
		DamageSlot->SetHorizontalAlignment(HAlign_Right);
		DamageSlot->SetVerticalAlignment(VAlign_Top);
	}

	SetIsNightmare(false);
}

void UNightlightCrayonHealthBar::SetIsNightmare(const bool bInIsNightmare)
{
	EnsureBuilt();

	// The fill is the yellow hatch tinted, so one tile serves both sides.
	HealthBar->SetFillColorAndOpacity(bInIsNightmare ? NightlightCrayonStyle::NightmareRed() : NightlightCrayonStyle::Grass());
}

void UNightlightCrayonHealthBar::ShowDamage(const float DamageAmount)
{
	EnsureBuilt();
	DamageText->SetText(FText::AsNumber(-FMath::RoundToInt(DamageAmount)));
	DamageText->SetVisibility(ESlateVisibility::HitTestInvisible);
	DamageAge = 0.0f;
}

void UNightlightCrayonHealthBar::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (DamageAge < 0.0f)
	{
		return;
	}

	// The number drips upwards and fades, so stacked hits never pile up into a block of text.
	DamageAge += InDeltaTime;
	const float Progress = FMath::Clamp(DamageAge / DamageSeconds, 0.0f, 1.0f);
	DamageText->SetRenderTranslation(FVector2D(0.0f, -14.0f * Progress));
	DamageText->SetRenderOpacity(1.0f - Progress * Progress);
	if (Progress >= 1.0f)
	{
		DamageText->SetVisibility(ESlateVisibility::Hidden);
		DamageAge = -1.0f;
	}
}

/*
References

Epic Games, Inc., 2026. UUserWidget::GetWidgetFromName. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/Blueprint/UUserWidget/GetWidgetFromName>
[Accessed 3 November 2026].
*/
