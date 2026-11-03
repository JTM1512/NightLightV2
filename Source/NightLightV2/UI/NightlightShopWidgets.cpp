#include "NightlightShopWidgets.h"
#include "NightlightSanityLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	const FVector2D CardSize(150.0f, 190.0f);

	// A button that draws nothing itself, so the paper card underneath is what the player sees.
	FButtonStyle MakeInvisibleButtonStyle()
	{
		FSlateBrush Empty;
		Empty.DrawAs = ESlateBrushDrawType::NoDrawType;

		FButtonStyle Style;
		Style.SetNormal(Empty);
		Style.SetHovered(Empty);
		Style.SetPressed(Empty);
		Style.SetDisabled(Empty);
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
		return Style;
	}

	void FillOverlaySlot(UOverlaySlot* const OverlaySlot, const FMargin& Padding = FMargin(0.0f))
	{
		if (OverlaySlot)
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Fill);
			OverlaySlot->SetPadding(Padding);
		}
	}
}

void UNightlightDrawingCard::BuildTree()
{
	UButton* const Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	WidgetTree->RootWidget = Button;
	Button->SetStyle(MakeInvisibleButtonStyle());
	Button->OnClicked.AddDynamic(this, &UNightlightDrawingCard::HandleClicked);

	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(CardSize.X);
	SizeBox->SetHeightOverride(CardSize.Y);
	if (UButtonSlot* const SizeSlot = Cast<UButtonSlot>(Button->AddChild(SizeBox)))
	{
		SizeSlot->SetPadding(FMargin(0.0f));
	}

	UOverlay* const Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SizeBox->AddChild(Layers);

	UImage* const Paper = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Paper->SetBrush(NightlightCrayonStyle::MakeBoxBrush(NightlightCrayonStyle::LoadTexture(TEXT("Panels/T_Paper_Card")), 0.12f));
	Paper->SetVisibility(ESlateVisibility::HitTestInvisible);
	FillOverlaySlot(Layers->AddChildToOverlay(Paper));

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	FillOverlaySlot(Layers->AddChildToOverlay(Column), FMargin(10.0f, 14.0f, 10.0f, 10.0f));

	Drawing = WidgetTree->ConstructWidget<UNightlightBoilImage>(UNightlightBoilImage::StaticClass());
	Drawing->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UVerticalBoxSlot* const DrawingSlot = Column->AddChildToVerticalBox(Drawing))
	{
		DrawingSlot->SetHorizontalAlignment(HAlign_Center);
	}

	NameText = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Kid, 24, NightlightCrayonStyle::Pencil());
	if (UVerticalBoxSlot* const NameSlot = Column->AddChildToVerticalBox(NameText))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UHorizontalBox* const Price = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	PriceRow = Price;
	if (UVerticalBoxSlot* const PriceSlot = Column->AddChildToVerticalBox(Price))
	{
		PriceSlot->SetHorizontalAlignment(HAlign_Center);
	}

	if (UHorizontalBoxSlot* const StarSlot = Price->AddChildToHorizontalBox(
		MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("Icons/HUD/T_Icon_StarToken")), FVector2D(26.0f))))
	{
		StarSlot->SetVerticalAlignment(VAlign_Center);
		StarSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
	}

	// Prices stay in the kid's font even at low sanity, so they are always readable.
	CostText = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Kid, 24, NightlightCrayonStyle::Pencil());
	if (UHorizontalBoxSlot* const CostSlot = Price->AddChildToHorizontalBox(CostText))
	{
		CostSlot->SetVerticalAlignment(VAlign_Center);
	}

	HotkeyText = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Kid, 26, NightlightCrayonStyle::Sky());

	// It sits partly over the dark map now, so a paper coloured outline keeps the blue readable.
	FSlateFontInfo HotkeyFont = NightlightCrayonStyle::GetFont(ENightlightCrayonFont::Kid, 26);
	HotkeyFont.OutlineSettings.OutlineSize = 2;
	HotkeyFont.OutlineSettings.OutlineColor = NightlightCrayonStyle::Paper();
	HotkeyText->SetFont(HotkeyFont);
	if (UOverlaySlot* const HotkeySlot = Layers->AddChildToOverlay(HotkeyText))
	{
		HotkeySlot->SetHorizontalAlignment(HAlign_Left);
		HotkeySlot->SetVerticalAlignment(VAlign_Top);
		// Negative padding hangs the number off the card's top left corner, clear of the drawing and name.
		HotkeySlot->SetPadding(FMargin(-14.0f, -22.0f, 0.0f, 0.0f));
	}

	ScribbleOut = MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("Overlays/T_ScribbleOut_Grey")), CardSize);
	ScribbleOut->SetVisibility(ESlateVisibility::Collapsed);
	FillOverlaySlot(Layers->AddChildToOverlay(ScribbleOut));

	// The circle is bigger than the card, so it is drawn past the card's edges.
	PickCircle = MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("Overlays/T_Pick_Circle_Red")), FVector2D(190.0f, 226.0f));
	PickCircle->SetVisibility(ESlateVisibility::Collapsed);
	FillOverlaySlot(Layers->AddChildToOverlay(PickCircle), FMargin(-20.0f, -18.0f, -20.0f, -18.0f));
}

void UNightlightDrawingCard::Setup(
	const int32 InCardIndex,
	const FString& DrawingPath,
	const FText& InName,
	const FText& Hotkey)
{
	EnsureBuilt();
	CardIndex = InCardIndex;
	CardName = InName;
	Drawing->SetFrames(NightlightCrayonStyle::LoadBoilFrames(DrawingPath), FVector2D(118.0f, 94.0f));
	NameText->SetText(InName);
	HotkeyText->SetText(Hotkey);
	TiltDegrees = FMath::FRandRange(-3.0f, 3.0f);
	SetRenderTransformAngle(TiltDegrees);
}

void UNightlightDrawingCard::SetCost(const int32 Cost)
{
	EnsureBuilt();
	if (Cost == ShownCost)
	{
		return;
	}

	ShownCost = Cost;
	PriceRow->SetVisibility(Cost >= 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	CostText->SetText(FText::AsNumber(FMath::Max(Cost, 0)));
}

void UNightlightDrawingCard::SetCardState(const bool bAffordable, const bool bPicked, const bool bDimmed, const bool bCorrupted)
{
	EnsureBuilt();
	const int32 StateBits = (bAffordable ? 1 : 0) | (bPicked ? 2 : 0) | (bDimmed ? 4 : 0) | (bCorrupted ? 8 : 0);
	if (StateBits == ShownStateBits)
	{
		return;
	}

	ShownStateBits = StateBits;
	ScribbleOut->SetVisibility(!bAffordable && !bDimmed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	PickCircle->SetVisibility(bPicked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bPicked && !bIsPicked)
	{
		// A kick upwards starts the spring, so the card jumps past its resting spot and settles back.
		LiftVelocity = -900.0f;
		ScaleVelocity = 6.0f;
		WiggleVelocity = FMath::RandBool() ? 420.0f : -420.0f;
	}
	bIsPicked = bPicked;
	SetRenderOpacity(bDimmed ? 0.45f : 1.0f);
	SetColorAndOpacity(bDimmed ? FLinearColor(0.6f, 0.6f, 0.6f) : FLinearColor::White);

	NameText->SetFont(NightlightCrayonStyle::GetLabelFont(bCorrupted, 24));
	NameText->SetText(bCorrupted ? UNightlightSanityLibrary::CorruptText(CardName) : CardName);
}

void UNightlightDrawingCard::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// A bouncy damped spring: the picked card rests 34 px higher and 20% bigger, wiggles, and straightens up,
	// so it looks lifted off the shelf. Real time is used, so it still pops while the game is sped up.
	const float DeltaTime = FMath::Min(InDeltaTime, 1.0f / 30.0f);
	const float TargetLift = bIsPicked ? -34.0f : 0.0f;
	const float TargetScale = bIsPicked ? 1.2f : 1.0f;
	const float Stiffness = 300.0f;
	const float Damping = 11.0f;

	LiftVelocity += ((TargetLift - Lift) * Stiffness - LiftVelocity * Damping) * DeltaTime;
	Lift += LiftVelocity * DeltaTime;
	ScaleVelocity += ((TargetScale - Scale) * Stiffness - ScaleVelocity * Damping) * DeltaTime;
	Scale += ScaleVelocity * DeltaTime;

	FWidgetTransform Transform;
	Transform.Translation = FVector2D(0.0f, Lift);
	Transform.Scale = FVector2D(Scale);
	WiggleVelocity += (-Wiggle * Stiffness - WiggleVelocity * Damping) * DeltaTime;
	Wiggle += WiggleVelocity * DeltaTime;
	Transform.Angle = (bIsPicked ? 0.0f : TiltDegrees) + Wiggle;
	SetRenderTransform(Transform);
}

void UNightlightDrawingCard::HandleClicked()
{
	OnCardClicked.Broadcast(CardIndex);
}

namespace
{
	// The page keeps the texture's 840x1160 shape. The text column starts right of the red margin line,
	// which sits about 17% in from the left edge, so nothing is written over the punched holes.
	const FVector2D PageSize(460.0f, 636.0f);
	const FMargin PageColumnPadding(84.0f, 36.0f, 36.0f, 34.0f);
}

void UNightlightNotebookPage::BuildTree()
{
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WidgetTree->RootWidget = SizeBox;
	SizeBox->SetWidthOverride(PageSize.X);
	SizeBox->SetHeightOverride(PageSize.Y);

	UOverlay* const Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SizeBox->AddChild(Layers);
	FillOverlaySlot(Layers->AddChildToOverlay(
		MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("Panels/T_Notebook_Page")), PageSize)));

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	FillOverlaySlot(Layers->AddChildToOverlay(Column), PageColumnPadding);

	TitleText = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Kid, 42, NightlightCrayonStyle::Sky());
	Column->AddChildToVerticalBox(TitleText);

	Portrait = WidgetTree->ConstructWidget<UNightlightBoilImage>(UNightlightBoilImage::StaticClass());
	Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UVerticalBoxSlot* const PortraitSlot = Column->AddChildToVerticalBox(Portrait))
	{
		PortraitSlot->SetHorizontalAlignment(HAlign_Center);
		PortraitSlot->SetPadding(FMargin(0.0f, 2.0f));
	}

	StarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* const StarSlot = Column->AddChildToVerticalBox(StarRow))
	{
		StarSlot->SetHorizontalAlignment(HAlign_Center);
		StarSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 10.0f));
	}

	StatList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* const StatSlot = Column->AddChildToVerticalBox(StatList))
	{
		StatSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	if (UVerticalBoxSlot* const SpacerSlot = Column->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass())))
	{
		SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// The price sits on its own line above the button, so the button label stays short enough to fit.
	UHorizontalBox* const Price = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	CostRow = Price;
	if (UVerticalBoxSlot* const PriceSlot = Column->AddChildToVerticalBox(Price))
	{
		PriceSlot->SetHorizontalAlignment(HAlign_Center);
		PriceSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	if (UHorizontalBoxSlot* const CostsSlot = Price->AddChildToHorizontalBox(
		MakeText(NSLOCTEXT("NightlightCrayon", "NotebookCosts", "costs"), ENightlightCrayonFont::Kid, 26, NightlightCrayonStyle::Pencil())))
	{
		CostsSlot->SetVerticalAlignment(VAlign_Center);
		CostsSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	}

	if (UHorizontalBoxSlot* const PriceStarSlot = Price->AddChildToHorizontalBox(
		MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("Icons/HUD/T_Icon_StarToken")), FVector2D(30.0f))))
	{
		PriceStarSlot->SetVerticalAlignment(VAlign_Center);
		PriceStarSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
	}

	CostText = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Kid, 30, NightlightCrayonStyle::Pencil());
	if (UHorizontalBoxSlot* const CostSlot = Price->AddChildToHorizontalBox(CostText))
	{
		CostSlot->SetVerticalAlignment(VAlign_Center);
	}

	HintText = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Kid, 22, NightlightCrayonStyle::NightmareRed());
	HintText->SetJustification(ETextJustify::Center);
	HintText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* const HintSlot = Column->AddChildToVerticalBox(HintText))
	{
		HintSlot->SetHorizontalAlignment(HAlign_Fill);
		HintSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	UpgradeButton = CreateWidget<UNightlightCrayonButton>(this, UNightlightCrayonButton::StaticClass());
	UpgradeButton->Setup(ENightlightCrayonButtonKind::Buy, NSLOCTEXT("NightlightCrayon", "NotebookUpgrade", "MAKE IT STRONGER!"), 26);
	UpgradeButton->OnClicked.AddDynamic(this, &UNightlightNotebookPage::HandleUpgrade);
	if (UVerticalBoxSlot* const UpgradeSlot = Column->AddChildToVerticalBox(UpgradeButton))
	{
		UpgradeSlot->SetHorizontalAlignment(HAlign_Fill);
		UpgradeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	CloseButton = CreateWidget<UNightlightCrayonButton>(this, UNightlightCrayonButton::StaticClass());
	CloseButton->Setup(ENightlightCrayonButtonKind::Secondary, NSLOCTEXT("NightlightCrayon", "NotebookClose", "let go (U)"), 22);
	CloseButton->OnClicked.AddDynamic(this, &UNightlightNotebookPage::HandleClose);
	if (UVerticalBoxSlot* const CloseSlot = Column->AddChildToVerticalBox(CloseButton))
	{
		CloseSlot->SetHorizontalAlignment(HAlign_Center);
	}

	SetRenderTransformAngle(1.5f);
}

void UNightlightNotebookPage::SetInfo(const FNightlightNotebookInfo& Info)
{
	EnsureBuilt();

	FString Signature = FString::Printf(TEXT("%s|%s|%d|%d|%d|%d"),
		*Info.Title.ToString(), *Info.PortraitPath, Info.Level, Info.MaxLevel, Info.Cost, Info.bCanAfford ? 1 : 0);
	for (const FText& Line : Info.StatLines)
	{
		Signature += TEXT("|") + Line.ToString();
	}

	if (Signature == ShownSignature)
	{
		return;
	}

	ShownSignature = MoveTemp(Signature);
	TitleText->SetText(Info.Title);

	if (Info.PortraitPath != ShownPortrait)
	{
		Portrait->SetFrames(NightlightCrayonStyle::LoadBoilFrames(Info.PortraitPath), FVector2D(150.0f, 120.0f));
		ShownPortrait = Info.PortraitPath;
	}

	// Gold stars for bought tiers, a dashed pencil star for the next one, grey for the rest.
	StarRow->ClearChildren();
	for (int32 Tier = 0; Tier < Info.MaxLevel; ++Tier)
	{
		const TCHAR* const StarPath = Tier < Info.Level ? TEXT("Icons/HUD/T_Icon_StarToken")
			: Tier == Info.Level ? TEXT("Icons/HUD/T_Icon_Star_Next")
			: TEXT("Icons/HUD/T_Icon_Star_Empty");
		if (UHorizontalBoxSlot* const StarSlot = StarRow->AddChildToHorizontalBox(MakeImage(NightlightCrayonStyle::LoadTexture(StarPath), FVector2D(44.0f))))
		{
			StarSlot->SetPadding(FMargin(3.0f, 0.0f));
		}
	}

	// Each stat wraps inside the column instead of running off the page.
	StatList->ClearChildren();
	for (const FText& Line : Info.StatLines)
	{
		UTextBlock* const LineText = MakeText(Line, ENightlightCrayonFont::Kid, 26, NightlightCrayonStyle::Pencil());
		LineText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* const LineSlot = StatList->AddChildToVerticalBox(LineText))
		{
			LineSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	const bool bMaxed = Info.Cost < 0;
	CostRow->SetVisibility(bMaxed ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	CostText->SetText(FText::AsNumber(FMath::Max(Info.Cost, 0)));

	FText Hint;
	if (bMaxed)
	{
		Hint = NSLOCTEXT("NightlightCrayon", "NotebookMaxedHint", "every star is coloured in!");
		HintText->SetColorAndOpacity(FSlateColor(NightlightCrayonStyle::Grass()));
	}
	else if (!Info.bCanAfford)
	{
		Hint = NSLOCTEXT("NightlightCrayon", "NotebookShort", "need more stars...");
		HintText->SetColorAndOpacity(FSlateColor(NightlightCrayonStyle::NightmareRed()));
	}
	HintText->SetText(Hint);
	HintText->SetVisibility(Hint.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	UpgradeButton->SetLabel(bMaxed
		? NSLOCTEXT("NightlightCrayon", "NotebookMaxed", "THE STRONGEST!")
		: NSLOCTEXT("NightlightCrayon", "NotebookUpgrade", "MAKE IT STRONGER!"));
	UpgradeButton->SetButtonEnabled(!bMaxed && Info.bCanAfford);
}

void UNightlightNotebookPage::HandleUpgrade()
{
	OnUpgradeClicked.Broadcast();
}

void UNightlightNotebookPage::HandleClose()
{
	OnCloseClicked.Broadcast();
}

void UNightlightNightmareText::BuildTree()
{
	MessageText = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Nightmare, 40, NightlightCrayonStyle::NightmareRed());
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetShadowOffset(FVector2D(2.0f, 3.0f));
	MessageText->SetShadowColorAndOpacity(NightlightCrayonStyle::Ink());
	WidgetTree->RootWidget = MessageText;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UNightlightNightmareText::ShowMessage(const FText& Message, const float Seconds, const int32 FontSize)
{
	EnsureBuilt();
	MessageText->SetText(Message);
	MessageText->SetFont(NightlightCrayonStyle::GetFont(ENightlightCrayonFont::Nightmare, FontSize));
	TimeLeft = Seconds;
	SetRenderOpacity(1.0f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UNightlightNightmareText::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (TimeLeft <= 0.0f)
	{
		return;
	}

	TimeLeft -= InDeltaTime;
	JitterClock += InDeltaTime;

	// A few pixels of jitter about 20 times a second makes the writing look like it is shaking.
	if (JitterClock >= 0.05f)
	{
		JitterClock = 0.0f;
		SetRenderTranslation(FVector2D(FMath::FRandRange(-3.0f, 3.0f), FMath::FRandRange(-2.0f, 2.0f)));
	}

	SetRenderOpacity(FMath::Clamp(TimeLeft / 0.6f, 0.0f, 1.0f));
	if (TimeLeft <= 0.0f)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}
