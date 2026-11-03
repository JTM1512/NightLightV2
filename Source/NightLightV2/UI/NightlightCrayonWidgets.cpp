#include "NightlightCrayonWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UNightlightCrayonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureBuilt();
}

TSharedRef<SWidget> UNightlightCrayonWidget::RebuildWidget()
{
	EnsureBuilt();
	return Super::RebuildWidget();
}

void UNightlightCrayonWidget::EnsureBuilt()
{
	// The tree is made in code with WidgetTree->ConstructWidget, the same way the designer does it
	// (Epic Games, Inc., 2026c).
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
}

UTextBlock* UNightlightCrayonWidget::MakeText(
	const FText& Text,
	const ENightlightCrayonFont Font,
	const int32 Size,
	const FLinearColor& Colour)
{
	UTextBlock* const TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextBlock->SetText(Text);
	TextBlock->SetFont(NightlightCrayonStyle::GetFont(Font, Size));
	TextBlock->SetColorAndOpacity(FSlateColor(Colour));
	return TextBlock;
}

UImage* UNightlightCrayonWidget::MakeImage(UTexture2D* const Texture, const FVector2D& Size)
{
	UImage* const Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Image->SetBrush(NightlightCrayonStyle::MakeImageBrush(Texture, Size));
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Image;
}

void UNightlightBoilImage::SetFrames(const TArray<UTexture2D*>& InFrames, const FVector2D& Size)
{
	Frames.Reset();
	for (UTexture2D* const Frame : InFrames)
	{
		Frames.Add(Frame);
	}

	SetBrush(NightlightCrayonStyle::MakeImageBrush(Frames.IsEmpty() ? nullptr : Frames[0].Get(), Size));
	ShownFrame = Frames.IsEmpty() ? INDEX_NONE : 0;
	GetLiveImages().AddUnique(this);
}

void UNightlightBoilImage::TickAll(const float DeltaTime, const float SpeedScale)
{
	float& Clock = GetClock();
	Clock += DeltaTime * FMath::Max(SpeedScale, 0.0f);

	TArray<TWeakObjectPtr<UNightlightBoilImage>>& LiveImages = GetLiveImages();
	for (int32 Index = LiveImages.Num() - 1; Index >= 0; --Index)
	{
		if (UNightlightBoilImage* const Image = LiveImages[Index].Get())
		{
			Image->ShowFrameForClock(Clock);
		}
		else
		{
			LiveImages.RemoveAtSwap(Index);
		}
	}
}

void UNightlightBoilImage::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	GetLiveImages().Remove(this);
}

void UNightlightBoilImage::ShowFrameForClock(const float Clock)
{
	if (Frames.Num() < 2)
	{
		return;
	}

	const int32 Frame = FMath::FloorToInt(Clock * FramesPerSecond) % Frames.Num();
	if (Frame != ShownFrame)
	{
		// Swapping only the resource keeps the brush size and tint the widget was given
		// (Epic Games, Inc., 2026b).
		SetBrushResourceObject(Frames[Frame]);
		ShownFrame = Frame;
	}
}

TArray<TWeakObjectPtr<UNightlightBoilImage>>& UNightlightBoilImage::GetLiveImages()
{
	static TArray<TWeakObjectPtr<UNightlightBoilImage>> LiveImages;
	return LiveImages;
}

float& UNightlightBoilImage::GetClock()
{
	static float Clock = 0.0f;
	return Clock;
}

void UNightlightPaperPanel::BuildTree()
{
	Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	PaperImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	PaperImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* const PaperSlot = Root->AddChildToOverlay(PaperImage))
	{
		PaperSlot->SetHorizontalAlignment(HAlign_Fill);
		PaperSlot->SetVerticalAlignment(VAlign_Fill);
	}

	TapeImage = MakeImage(nullptr, FVector2D(84.0f, 30.0f));
	TapeImage->SetVisibility(ESlateVisibility::Collapsed);
}

void UNightlightPaperPanel::Setup(
	const FString& PaperPath,
	const float Margin,
	const FMargin& ContentPadding,
	const bool bWithTape,
	const float MaxTiltDegrees)
{
	EnsureBuilt();
	PanelPadding = ContentPadding;
	PaperImage->SetBrush(NightlightCrayonStyle::MakeBoxBrush(NightlightCrayonStyle::LoadTexture(PaperPath), Margin));

	if (bWithTape)
	{
		const FString TapePath = FString::Printf(TEXT("Panels/T_Tape_0%d"), FMath::RandRange(1, 3));
		TapeImage->SetBrush(NightlightCrayonStyle::MakeImageBrush(NightlightCrayonStyle::LoadTexture(TapePath), FVector2D(84.0f, 30.0f)));
		TapeImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		TapeImage->SetRenderTransformAngle(FMath::FRandRange(-8.0f, 8.0f));
	}

	SetRenderTransformAngle(FMath::FRandRange(-MaxTiltDegrees, MaxTiltDegrees));
}

void UNightlightPaperPanel::SetContent(UWidget* const Content)
{
	EnsureBuilt();
	if (!Content)
	{
		return;
	}

	if (UOverlaySlot* const ContentSlot = Root->AddChildToOverlay(Content))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
		ContentSlot->SetPadding(PanelPadding);
	}

	// Tape goes on last so it sits over the content, half off the top edge.
	Root->RemoveChild(TapeImage);
	if (UOverlaySlot* const TapeSlot = Root->AddChildToOverlay(TapeImage))
	{
		TapeSlot->SetHorizontalAlignment(HAlign_Center);
		TapeSlot->SetVerticalAlignment(VAlign_Top);
		TapeSlot->SetPadding(FMargin(0.0f, -14.0f, 0.0f, 0.0f));
	}
}

void UNightlightCrayonButton::BuildTree()
{
	Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	WidgetTree->RootWidget = Button;
	Button->OnClicked.AddDynamic(this, &UNightlightCrayonButton::HandleClicked);

	LabelText = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Kid, 26, NightlightCrayonStyle::Pencil());
	LabelText->SetJustification(ETextJustify::Center);
	LabelText->SetAutoWrapText(true);
	if (UButtonSlot* const LabelSlot = Cast<UButtonSlot>(Button->AddChild(LabelText)))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(24.0f, 6.0f));
	}
}

void UNightlightCrayonButton::Setup(const ENightlightCrayonButtonKind Kind, const FText& Label, const int32 FontSize)
{
	EnsureBuilt();
	const bool bBuy = Kind == ENightlightCrayonButtonKind::Buy;
	UTexture2D* const NormalTexture = NightlightCrayonStyle::LoadTexture(bBuy ? TEXT("Panels/T_Button_Crayon_Green") : TEXT("Panels/T_Button_Crayon_Blue"));
	UTexture2D* const HoverTexture = bBuy ? NightlightCrayonStyle::LoadTexture(TEXT("Panels/T_Button_Crayon_Green_Hover")) : NormalTexture;

	// The pressed padding moves the label down, so the crayon looks pushed into the paper
	// (Epic Games, Inc., 2026a).
	FButtonStyle Style;
	Style.SetNormal(NightlightCrayonStyle::MakeBoxBrush(NormalTexture, 0.2f));
	FSlateBrush HoverBrush = NightlightCrayonStyle::MakeBoxBrush(HoverTexture, 0.2f);
	if (!bBuy)
	{
		HoverBrush.TintColor = FSlateColor(FLinearColor(1.15f, 1.15f, 1.15f));
	}
	Style.SetHovered(HoverBrush);
	FSlateBrush PressedBrush = HoverBrush;
	PressedBrush.TintColor = FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f));
	Style.SetPressed(PressedBrush);
	Style.SetDisabled(NightlightCrayonStyle::MakeBoxBrush(NightlightCrayonStyle::LoadTexture(TEXT("Panels/T_Button_Crayon_Disabled")), 0.2f));
	Style.SetNormalPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	Style.SetPressedPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
	Button->SetStyle(Style);

	LabelText->SetFont(NightlightCrayonStyle::GetFont(ENightlightCrayonFont::Kid, FontSize));
	LabelText->SetColorAndOpacity(FSlateColor(bBuy ? NightlightCrayonStyle::Pencil() : FLinearColor::White));
	SetLabel(Label);
}

void UNightlightCrayonButton::SetLabel(const FText& Label)
{
	EnsureBuilt();
	LabelText->SetText(Label);
}

void UNightlightCrayonButton::SetButtonEnabled(const bool bEnabled)
{
	EnsureBuilt();
	Button->SetIsEnabled(bEnabled);
	LabelText->SetRenderOpacity(bEnabled ? 1.0f : 0.55f);
}

void UNightlightCrayonButton::HandleClicked()
{
	OnClicked.Broadcast();
}

/*
References

Epic Games, Inc., 2026a. FButtonStyle. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/SlateCore/Styling/FButtonStyle>
[Accessed 3 November 2026].

Epic Games, Inc., 2026b. UImage. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UImage>
[Accessed 3 November 2026].

Epic Games, Inc., 2026c. UWidgetTree. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UWidgetTree>
[Accessed 3 November 2026].
*/
