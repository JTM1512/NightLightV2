#include "NightlightCrayonHUDWidget.h"
#include "NightlightBlueprintBridge.h"
#include "NightlightCrayonScreens.h"
#include "NightlightSanityWidgets.h"
#include "../Core/NightlightDreamCore.h"
#include "../Defenders/NightlightDefender.h"
#include "../Enemies/NightlightEnemy.h"
#include "../Enemies/NightlightEnemyBrute.h"
#include "../Enemies/NightlightEnemyShade.h"
#include "../Enemies/NightlightEnemySpawner.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/RetainerBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const TCHAR* const PlatformClassPath = TEXT("/Game/Blueprints/BP_DefenderPlacement.BP_DefenderPlacement_C");
	const int32 CoreCardIndex = 3;
	const int32 MaxPlatformMarks = 32;

	FButtonStyle MakeClearButtonStyle()
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

	UClass* GetPlatformClass()
	{
		static TWeakObjectPtr<UClass> PlatformClass;
		if (!PlatformClass.IsValid())
		{
			PlatformClass = LoadClass<AActor>(nullptr, PlatformClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}

		return PlatformClass.Get();
	}

	FText DescribeDirection(const FVector& Direction, const FRotator& CameraRotation)
	{
		// Directions are from the player's point of view, so "east" is always the right of the screen.
		const FVector Right = FRotationMatrix(FRotator(0.0f, CameraRotation.Yaw, 0.0f)).GetUnitAxis(EAxis::Y);
		const FVector Forward = FRotationMatrix(FRotator(0.0f, CameraRotation.Yaw, 0.0f)).GetUnitAxis(EAxis::X);
		const float Across = FVector::DotProduct(Direction, Right);
		const float Ahead = FVector::DotProduct(Direction, Forward);
		if (FMath::Abs(Across) > FMath::Abs(Ahead))
		{
			return Across > 0.0f ? NSLOCTEXT("NightlightCrayon", "East", "EAST") : NSLOCTEXT("NightlightCrayon", "West", "WEST");
		}

		return Ahead > 0.0f ? NSLOCTEXT("NightlightCrayon", "North", "NORTH") : NSLOCTEXT("NightlightCrayon", "South", "SOUTH");
	}

	FText StatLine(const FText& Label, const float Now, const TOptional<float> Next)
	{
		if (!Next.IsSet() || FMath::IsNearlyEqual(Now, Next.GetValue()))
		{
			return FText::Format(NSLOCTEXT("NightlightCrayon", "StatNow", "{0}: {1}"), Label, FText::AsNumber(FMath::RoundToInt(Now)));
		}

		return FText::Format(
			NSLOCTEXT("NightlightCrayon", "StatNext", "{0}: {1} -> {2}!"),
			Label, FText::AsNumber(FMath::RoundToInt(Now)), FText::AsNumber(FMath::RoundToInt(Next.GetValue())));
	}
}

void UNightlightCrayonHUDWidget::BuildTree()
{
	ShelfEntries = {
		{ NSLOCTEXT("NightlightCrayon", "Patroller", "Patroller"), TEXT("Icons/Defenders/T_Defender_Patroller"),
			FSoftClassPath(TEXT("/Game/Blueprints/BP_Defender.BP_Defender_C")), TEXT("PatrollerCost"), FText::FromString(TEXT("1")) },
		{ NSLOCTEXT("NightlightCrayon", "Shooter", "Shooter"), TEXT("Icons/Defenders/T_Defender_Shooter"),
			FSoftClassPath(TEXT("/Game/Blueprints/BP_DefenderShooter.BP_DefenderShooter_C")), TEXT("ShooterCost"), FText::FromString(TEXT("2")) },
		{ NSLOCTEXT("NightlightCrayon", "Torch", "Torch"), TEXT("Icons/Defenders/T_Defender_Torch"),
			FSoftClassPath(TEXT("/Game/Blueprints/BP_DefenderPulse.BP_DefenderPulse_C")), TEXT("PulseCost"), FText::FromString(TEXT("3")) },
		{ NSLOCTEXT("NightlightCrayon", "Heart", "Heart"), TEXT("Icons/Defenders/T_Defender_Core"),
			FSoftClassPath(), NAME_None, FText::FromString(TEXT("U")) },
	};

	// The Retainer Box draws the whole HUD into one texture, so a single material can wobble and drain
	// every widget at once, text included (Epic Games, Inc., 2026c).
	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	// The world markers stay outside the Retainer Box. It only redraws every other frame, so markers
	// inside it lagged a frame behind the camera and jittered while it moved.
	MarkerLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	MarkerLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* const MarkerSlot = Root->AddChildToOverlay(MarkerLayer))
	{
		MarkerSlot->SetHorizontalAlignment(HAlign_Fill);
		MarkerSlot->SetVerticalAlignment(VAlign_Fill);
	}

	Retainer = WidgetTree->ConstructWidget<URetainerBox>(URetainerBox::StaticClass());
	if (UOverlaySlot* const RetainerSlot = Root->AddChildToOverlay(Retainer))
	{
		RetainerSlot->SetHorizontalAlignment(HAlign_Fill);
		RetainerSlot->SetVerticalAlignment(VAlign_Fill);
	}
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Retainer->AddChild(Canvas);

	UMaterialInterface* const WarpBase = bUseSanityWarp ? NightlightCrayonStyle::LoadMaterial(TEXT("M_UI_SanityWarp")) : nullptr;
	if (WarpBase)
	{
		Retainer->SetEffectMaterial(WarpBase);
		Retainer->SetTextureParameter(TEXT("Texture"));
		WarpMaterial = Retainer->GetEffectMaterial();

		// Every other frame is about 30 updates a second at 60 fps, which hides the wobble's cost.
		Retainer->SetRenderingPhase(0, 2);
		Retainer->SetRetainRendering(true);
	}
	else
	{
		Retainer->SetRetainRendering(false);
	}

	// The ink goes in first, so the sanity bar, tokens and shelf always draw on top of it.
	InkOverlay = CreateWidget<UNightlightInkOverlay>(this, UNightlightInkOverlay::StaticClass());
	if (UCanvasPanelSlot* const InkSlot = Canvas->AddChildToCanvas(InkOverlay))
	{
		InkSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		InkSlot->SetOffsets(FMargin(0.0f));
	}

	BuildMarkers();
	BuildTopLeft();
	BuildTopRight();
	BuildBottom();
	BuildMessages();
}

UCanvasPanelSlot* UNightlightCrayonHUDWidget::AddToCanvas(
	UWidget* const Widget,
	const FAnchors& Anchors,
	const FVector2D& Alignment,
	const FVector2D& Position)
{
	UCanvasPanelSlot* const CanvasSlot = Canvas->AddChildToCanvas(Widget);
	if (CanvasSlot)
	{
		CanvasSlot->SetAnchors(Anchors);
		CanvasSlot->SetAlignment(Alignment);
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetAutoSize(true);
	}

	return CanvasSlot;
}

void UNightlightCrayonHUDWidget::BuildMarkers()
{
	auto AddRing = [this](const TCHAR* const TexturePath)
	{
		UImage* const Ring = MakeImage(NightlightCrayonStyle::LoadTexture(TexturePath), FVector2D(256.0f));
		Ring->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* const RingSlot = MarkerLayer->AddChildToCanvas(Ring))
		{
			RingSlot->SetAlignment(FVector2D(0.5f));
		}
		return Ring;
	};

	RangeRing = AddRing(TEXT("Overlays/T_Range_Ring_Yellow"));
	SelectedRing = AddRing(TEXT("Overlays/T_Range_Ring_Blue"));

	NotebookArrow = MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("Overlays/T_Arrow_Crayon")), FVector2D(150.0f, 75.0f));
	NotebookArrow->SetVisibility(ESlateVisibility::Collapsed);
	NotebookArrow->SetRenderTransformPivot(FVector2D(0.5f));
	if (UCanvasPanelSlot* const ArrowSlot = MarkerLayer->AddChildToCanvas(NotebookArrow))
	{
		ArrowSlot->SetAlignment(FVector2D(0.5f));
		ArrowSlot->SetAutoSize(true);
	}

	UVerticalBox* const Next = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	NextDrawing = WidgetTree->ConstructWidget<UNightlightBoilImage>(UNightlightBoilImage::StaticClass());
	if (UVerticalBoxSlot* const DrawingSlot = Next->AddChildToVerticalBox(NextDrawing))
	{
		DrawingSlot->SetHorizontalAlignment(HAlign_Center);
	}

	NextText = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Kid, 24, NightlightCrayonStyle::Sun());
	NextText->SetShadowOffset(FVector2D(2.0f, 2.0f));
	NextText->SetShadowColorAndOpacity(NightlightCrayonStyle::Ink());
	if (UVerticalBoxSlot* const TextSlot = Next->AddChildToVerticalBox(NextText))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Center);
	}

	Next->SetVisibility(ESlateVisibility::Collapsed);
	Next->SetRenderOpacity(0.9f);
	NextMarker = Next;
	if (UCanvasPanelSlot* const NextSlot = MarkerLayer->AddChildToCanvas(Next))
	{
		NextSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		NextSlot->SetAutoSize(true);
	}
}

void UNightlightCrayonHUDWidget::BuildTopLeft()
{
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* const TokenRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(TokenRow);

	if (UHorizontalBoxSlot* const StarSlot = TokenRow->AddChildToHorizontalBox(
		MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("Icons/HUD/T_Icon_StarToken")), FVector2D(52.0f))))
	{
		StarSlot->SetVerticalAlignment(VAlign_Center);
		StarSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}

	TokenText = MakeText(FText::AsNumber(0), ENightlightCrayonFont::Kid, 44, NightlightCrayonStyle::Pencil());
	if (UHorizontalBoxSlot* const TokenSlot = TokenRow->AddChildToHorizontalBox(TokenText))
	{
		TokenSlot->SetVerticalAlignment(VAlign_Center);
	}

	SlotFigures = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* const FigureSlot = Column->AddChildToVerticalBox(SlotFigures))
	{
		FigureSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
	}

	UNightlightPaperPanel* const Panel = CreateWidget<UNightlightPaperPanel>(this, UNightlightPaperPanel::StaticClass());
	Panel->Setup(TEXT("Panels/T_Paper_Scrap"), 0.1f, FMargin(22.0f, 10.0f, 26.0f, 14.0f), true);
	Panel->SetContent(Column);
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToCanvas(Panel, FAnchors(0.0f, 0.0f), FVector2D(0.0f), FVector2D(24.0f, 20.0f));

	SanityBar = CreateWidget<UNightlightSanityBar>(this, UNightlightSanityBar::StaticClass());
	AddToCanvas(SanityBar, FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 10.0f));
}

void UNightlightCrayonHUDWidget::BuildTopRight()
{
	WaveLabel = MakeText(NSLOCTEXT("NightlightCrayon", "WaveQuiet", "the night is quiet..."), ENightlightCrayonFont::Nightmare, 34, NightlightCrayonStyle::NightmareRed());
	WaveLabel->SetJustification(ETextJustify::Right);
	WaveLabel->SetShadowOffset(FVector2D(2.0f, 3.0f));
	WaveLabel->SetShadowColorAndOpacity(NightlightCrayonStyle::Ink());
	WaveLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToCanvas(WaveLabel, FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(-30.0f, 30.0f));
}

void UNightlightCrayonHUDWidget::BuildBottom()
{
	// The bedside lamp starts the night early. Its countdown is the only value read every frame.
	UButton* const Lamp = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Lamp->SetStyle(MakeClearButtonStyle());
	Lamp->OnClicked.AddDynamic(this, &UNightlightCrayonHUDWidget::HandleLampClicked);
	UVerticalBox* const LampColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Lamp->AddChild(LampColumn);

	UNightlightBoilImage* const LampDrawing = WidgetTree->ConstructWidget<UNightlightBoilImage>(UNightlightBoilImage::StaticClass());
	LampDrawing->SetFrames(NightlightCrayonStyle::LoadBoilFrames(TEXT("Icons/HUD/T_Button_Lamp")), FVector2D(140.0f, 150.0f));
	if (UVerticalBoxSlot* const LampSlot = LampColumn->AddChildToVerticalBox(LampDrawing))
	{
		LampSlot->SetHorizontalAlignment(HAlign_Center);
	}

	LampText = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Kid, 30, FLinearColor::White);
	LampText->SetShadowOffset(FVector2D(2.0f, 2.0f));
	LampText->SetShadowColorAndOpacity(NightlightCrayonStyle::Ink());
	if (UVerticalBoxSlot* const LampTextSlot = LampColumn->AddChildToVerticalBox(LampText))
	{
		LampTextSlot->SetHorizontalAlignment(HAlign_Center);
	}

	LampButton = Lamp;
	Lamp->SetVisibility(ESlateVisibility::Collapsed);
	AddToCanvas(Lamp, FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-30.0f, -20.0f));

	// During a wave the lamp swaps for a sticky note that speeds the game up.
	UButton* const Speed = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle SpeedStyle = MakeClearButtonStyle();
	FSlateBrush Note = NightlightCrayonStyle::MakeBoxBrush(NightlightCrayonStyle::LoadTexture(TEXT("Panels/T_StickyNote_Yellow")), 0.1f);
	SpeedStyle.SetNormal(Note);
	Note.TintColor = FSlateColor(FLinearColor(1.1f, 1.1f, 1.0f));
	SpeedStyle.SetHovered(Note);
	SpeedStyle.SetPressed(Note);
	Speed->SetStyle(SpeedStyle);
	Speed->OnClicked.AddDynamic(this, &UNightlightCrayonHUDWidget::HandleSpeedClicked);
	SpeedText = MakeText(NSLOCTEXT("NightlightCrayon", "Faster", "FASTER!! (F)"), ENightlightCrayonFont::Kid, 32, NightlightCrayonStyle::Pencil());
	if (UButtonSlot* const SpeedTextSlot = Cast<UButtonSlot>(Speed->AddChild(SpeedText)))
	{
		SpeedTextSlot->SetPadding(FMargin(34.0f, 26.0f));
	}
	Speed->SetRenderTransformAngle(-4.0f);
	Speed->SetVisibility(ESlateVisibility::Collapsed);
	SpeedNote = Speed;
	AddToCanvas(Speed, FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-30.0f, -30.0f));

	// The shelf of drawings along the bottom.
	UHorizontalBox* const Shelf = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 Index = 0; Index < ShelfEntries.Num(); ++Index)
	{
		UNightlightDrawingCard* const Card = CreateWidget<UNightlightDrawingCard>(this, UNightlightDrawingCard::StaticClass());
		Card->Setup(Index, ShelfEntries[Index].DrawingPath, ShelfEntries[Index].Name, ShelfEntries[Index].Hotkey);
		Card->OnCardClicked.AddDynamic(this, &UNightlightCrayonHUDWidget::HandleCardClicked);
		if (UHorizontalBoxSlot* const CardSlot = Shelf->AddChildToHorizontalBox(Card))
		{
			CardSlot->SetPadding(FMargin(Index == CoreCardIndex ? 34.0f : 10.0f, 0.0f, 10.0f, 0.0f));
		}
		Cards.Add(Card);
	}
	AddToCanvas(Shelf, FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -16.0f));

	SlotsFullHint = MakeText(
		NSLOCTEXT("NightlightCrayon", "SlotsFull", "no room for more friends! make the heart stronger ->"),
		ENightlightCrayonFont::Kid, 26, NightlightCrayonStyle::Sun());
	SlotsFullHint->SetShadowOffset(FVector2D(2.0f, 2.0f));
	SlotsFullHint->SetShadowColorAndOpacity(NightlightCrayonStyle::Ink());
	SlotsFullHint->SetVisibility(ESlateVisibility::Collapsed);
	AddToCanvas(SlotsFullHint, FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -222.0f));

	Notebook = CreateWidget<UNightlightNotebookPage>(this, UNightlightNotebookPage::StaticClass());
	Notebook->OnUpgradeClicked.AddDynamic(this, &UNightlightCrayonHUDWidget::HandleUpgradeClicked);
	Notebook->OnCloseClicked.AddDynamic(this, &UNightlightCrayonHUDWidget::CloseNotebook);
	Notebook->SetVisibility(ESlateVisibility::Collapsed);
	AddToCanvas(Notebook, FAnchors(1.0f, 0.5f), FVector2D(1.0f, 0.5f), FVector2D(-30.0f, -20.0f));
}

void UNightlightCrayonHUDWidget::BuildMessages()
{
	Warning = CreateWidget<UNightlightNightmareText>(this, UNightlightNightmareText::StaticClass());
	AddToCanvas(Warning, FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 150.0f));

	BigWarning = CreateWidget<UNightlightNightmareText>(this, UNightlightNightmareText::StaticClass());
	AddToCanvas(BigWarning, FAnchors(0.5f, 0.4f), FVector2D(0.5f), FVector2D(0.0f));

	// "WE DID IT!!" over the rainbow, with a paper scrap that sums the night up.
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UOverlay* const RainbowLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Column->AddChildToVerticalBox(RainbowLayer);
	RainbowLayer->AddChildToOverlay(MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("Overlays/T_Rainbow_WeDidIt")), FVector2D(640.0f, 180.0f)));
	UTextBlock* const Cheer = MakeText(NSLOCTEXT("NightlightCrayon", "WeDidIt", "WE DID IT!!"), ENightlightCrayonFont::Kid, 72, NightlightCrayonStyle::Sky());
	Cheer->SetShadowOffset(FVector2D(3.0f, 3.0f));
	Cheer->SetShadowColorAndOpacity(FLinearColor::White);
	if (UOverlaySlot* const CheerSlot = RainbowLayer->AddChildToOverlay(Cheer))
	{
		CheerSlot->SetHorizontalAlignment(HAlign_Center);
		CheerSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	WaveClearedSummary = MakeText(FText::GetEmpty(), ENightlightCrayonFont::Kid, 28, NightlightCrayonStyle::Pencil());
	UNightlightPaperPanel* const SummaryPanel = CreateWidget<UNightlightPaperPanel>(this, UNightlightPaperPanel::StaticClass());
	SummaryPanel->Setup(TEXT("Panels/T_Paper_Scrap"), 0.1f, FMargin(28.0f, 12.0f), true);
	SummaryPanel->SetContent(WaveClearedSummary);
	if (UVerticalBoxSlot* const SummarySlot = Column->AddChildToVerticalBox(SummaryPanel))
	{
		SummarySlot->SetHorizontalAlignment(HAlign_Center);
		SummarySlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}

	Column->SetVisibility(ESlateVisibility::Collapsed);
	WaveCleared = Column;
	AddToCanvas(Column, FAnchors(0.5f, 0.38f), FVector2D(0.5f), FVector2D(0.0f));
}

void UNightlightCrayonHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The controller starts with Selected Defender at 0, which would place a Patroller without any card
	// being picked. It is cleared on the first tick, once the controller has started.
	bClearControllerSelection = true;
}

void UNightlightCrayonHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ActorSearchTimer -= InDeltaTime;
	if ((!Core.IsValid() || !Director.IsValid() || !Spawner.IsValid()) && ActorSearchTimer <= 0.0f)
	{
		ActorSearchTimer = 0.5f;
		FindGameActors();
	}

	UpdateSanity(InDeltaTime);
	UpdateTokens();
	UpdateSlots();
	UpdateCountdown();
	if (bClearControllerSelection)
	{
		bClearControllerSelection = false;
		NightlightBlueprintBridge::WriteInt(GetOwningPlayer(), TEXT("SelectedDefender"), INDEX_NONE);
	}

	UpdateShelf();
	UpdateMarkers();
	UpdateStarPops(InDeltaTime);

	NotebookRefreshTimer -= InDeltaTime;
	if (NotebookRefreshTimer <= 0.0f)
	{
		NotebookRefreshTimer = 0.5f;
		UpdateNotebook();
	}

	if (WaveClearedTimeLeft > 0.0f)
	{
		WaveClearedTimeLeft -= InDeltaTime;
		WaveCleared->SetRenderOpacity(FMath::Clamp(WaveClearedTimeLeft / 0.5f, 0.0f, 1.0f));
		if (WaveClearedTimeLeft <= 0.0f)
		{
			WaveCleared->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	SweepTimer -= InDeltaTime;
	if (bReplaceOldWidgets && SweepTimer <= 0.0f)
	{
		SweepTimer = 0.25f;
		SweepOldWidgets();
	}

	// Lines boil faster as the kid gets more scared.
	const float BoilSpeed = SanityState == ENightlightSanityState::Brave ? 1.0f
		: SanityState == ENightlightSanityState::Scared ? 1.6f : 2.6f;
	UNightlightBoilImage::TickAll(InDeltaTime, BoilSpeed);
}

void UNightlightCrayonHUDWidget::NativeDestruct()
{
	if (Core.IsValid())
	{
		Core->OnCoreHealthChanged.RemoveAll(this);
		Core->OnCoreDestroyed.RemoveAll(this);
	}

	if (Director.IsValid())
	{
		Director->OnBuildPhaseStarted.RemoveAll(this);
		Director->OnWaveStarted.RemoveAll(this);
		Director->OnEnemiesRemainingChanged.RemoveAll(this);
		Director->OnWaveCleared.RemoveAll(this);
	}

	if (Spawner.IsValid())
	{
		Spawner->OnEnemySpawned.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UNightlightCrayonHUDWidget::FindGameActors()
{
	// Found once and kept, as the wave director's notes for the HUD suggest (Epic Games, Inc., 2026b).
	if (!Core.IsValid())
	{
		Core = Cast<ANightlightDreamCore>(UGameplayStatics::GetActorOfClass(this, ANightlightDreamCore::StaticClass()));
		if (Core.IsValid())
		{
			Core->OnCoreHealthChanged.AddDynamic(this, &UNightlightCrayonHUDWidget::HandleCoreHealthChanged);
			Core->OnCoreDestroyed.AddDynamic(this, &UNightlightCrayonHUDWidget::HandleCoreDestroyed);
			TargetSanity = Core->GetHealthPercent();
			DisplayedSanity = TargetSanity;
		}
	}

	if (!Director.IsValid())
	{
		Director = Cast<ANightlightWaveDirector>(UGameplayStatics::GetActorOfClass(this, ANightlightWaveDirector::StaticClass()));
		if (Director.IsValid())
		{
			Director->OnBuildPhaseStarted.AddDynamic(this, &UNightlightCrayonHUDWidget::HandleBuildPhaseStarted);
			Director->OnWaveStarted.AddDynamic(this, &UNightlightCrayonHUDWidget::HandleWaveStarted);
			Director->OnEnemiesRemainingChanged.AddDynamic(this, &UNightlightCrayonHUDWidget::HandleEnemiesRemainingChanged);
			Director->OnWaveCleared.AddDynamic(this, &UNightlightCrayonHUDWidget::HandleWaveCleared);
			EnemiesLeft = Director->GetEnemiesRemaining();
			RefreshWaveLabel();
		}
	}

	if (!Spawner.IsValid())
	{
		Spawner = Cast<ANightlightEnemySpawner>(UGameplayStatics::GetActorOfClass(this, ANightlightEnemySpawner::StaticClass()));
		if (Spawner.IsValid())
		{
			Spawner->OnEnemySpawned.AddDynamic(this, &UNightlightCrayonHUDWidget::HandleEnemySpawned);
		}
	}
}

void UNightlightCrayonHUDWidget::UpdateSanity(const float DeltaTime)
{
	DisplayedSanity = FMath::FInterpTo(DisplayedSanity, TargetSanity, DeltaTime, 3.0f);

	const ENightlightSanityState NewState = UNightlightSanityLibrary::GetSanityState(TargetSanity);
	if (NewState != SanityState)
	{
		SanityState = NewState;
		OnSanityStateChanged.Broadcast(SanityState);
		RefreshWaveLabel();

		const bool bInWave = Director.IsValid()
			&& Director->GetWavePhase() != ENightlightWavePhase::BuildPhase
			&& Director->GetWavePhase() != ENightlightWavePhase::NotStarted;
		if (SanityState == ENightlightSanityState::LosingIt && bInWave && !bWarnedThisWave)
		{
			bWarnedThisWave = true;
			BigWarning->ShowMessage(NSLOCTEXT("NightlightCrayon", "DontLetItIn", "DON'T LET IT IN"), 2.5f, 84);
		}
	}

	SanityBar->SetSanity(DisplayedSanity, SanityState);
	InkOverlay->UpdateInk(DisplayedSanity, SanityState, DeltaTime);
	if (WarpMaterial)
	{
		WarpMaterial->SetScalarParameterValue(TEXT("Sanity"), DisplayedSanity);
	}
}

void UNightlightCrayonHUDWidget::UpdateTokens()
{
	int32 NewTokens = 0;
	if (!NightlightBlueprintBridge::ReadInt(GetGameState(), TEXT("DreamTokens"), NewTokens) || NewTokens == Tokens)
	{
		return;
	}

	const int32 Change = Tokens == INDEX_NONE ? 0 : NewTokens - Tokens;
	Tokens = NewTokens;
	TokenText->SetText(FText::AsNumber(Tokens));
	OnTokensChanged.Broadcast(Tokens, Change);
	NotebookRefreshTimer = 0.0f;

	if (Change > 0)
	{
		UHorizontalBox* const Pop = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Pop->AddChildToHorizontalBox(MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("Icons/HUD/T_Icon_StarToken")), FVector2D(30.0f)));
		UTextBlock* const PopText = MakeText(
			FText::Format(NSLOCTEXT("NightlightCrayon", "StarPop", "+{0}"), FText::AsNumber(Change)),
			ENightlightCrayonFont::Kid, 30, NightlightCrayonStyle::Sun());
		PopText->SetShadowOffset(FVector2D(1.0f, 2.0f));
		PopText->SetShadowColorAndOpacity(NightlightCrayonStyle::Ink());
		Pop->AddChildToHorizontalBox(PopText);
		Pop->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddToCanvas(Pop, FAnchors(0.0f, 0.0f), FVector2D(0.0f), FVector2D(200.0f + FMath::FRandRange(-10.0f, 10.0f), 34.0f));
		StarPops.Add({ Pop, 0.0f });
	}
}

void UNightlightCrayonHUDWidget::UpdateStarPops(const float DeltaTime)
{
	for (int32 Index = StarPops.Num() - 1; Index >= 0; --Index)
	{
		FStarPop& StarPop = StarPops[Index];
		StarPop.Age += DeltaTime;
		UWidget* const Widget = StarPop.Widget.Get();
		if (!Widget || StarPop.Age >= 1.2f)
		{
			if (Widget)
			{
				Widget->RemoveFromParent();
			}
			StarPops.RemoveAtSwap(Index);
			continue;
		}

		Widget->SetRenderTranslation(FVector2D(0.0f, -40.0f * StarPop.Age));
		Widget->SetRenderOpacity(1.0f - FMath::Clamp((StarPop.Age - 0.6f) / 0.6f, 0.0f, 1.0f));
	}
}

void UNightlightCrayonHUDWidget::UpdateSlots()
{
	if (!Core.IsValid())
	{
		return;
	}

	const int32 Count = Core->GetDefenderCount();
	const int32 Max = Core->GetMaxDefenders();
	if (Count == ShownDefenderCount && Max == ShownMaxDefenders)
	{
		return;
	}

	// A new defender means one was just placed, so the picked card goes back on the shelf.
	if (ShownDefenderCount != INDEX_NONE && Count > ShownDefenderCount && GetSelectedCard() != INDEX_NONE)
	{
		ClearPickedCard();
	}

	ShownDefenderCount = Count;
	ShownMaxDefenders = Max;

	// One stick figure per slot, faded when it is free.
	SlotFigures->ClearChildren();
	UTexture2D* const On = NightlightCrayonStyle::LoadTexture(TEXT("Icons/HUD/T_Icon_SlotFigure_On"));
	UTexture2D* const Off = NightlightCrayonStyle::LoadTexture(TEXT("Icons/HUD/T_Icon_SlotFigure_Off"));
	for (int32 SlotIndex = 0; SlotIndex < Max; ++SlotIndex)
	{
		if (UHorizontalBoxSlot* const FigureSlot = SlotFigures->AddChildToHorizontalBox(MakeImage(SlotIndex < Count ? On : Off, FVector2D(22.0f, 33.0f))))
		{
			FigureSlot->SetPadding(FMargin(0.0f, 0.0f, 2.0f, 0.0f));
		}
	}
}

void UNightlightCrayonHUDWidget::UpdateCountdown()
{
	const ENightlightWavePhase Phase = Director.IsValid() ? Director->GetWavePhase() : ENightlightWavePhase::NotStarted;
	const bool bBuilding = Phase == ENightlightWavePhase::BuildPhase;
	const bool bInWave = !bBuilding && Phase != ENightlightWavePhase::NotStarted && !GameOverScreen;

	LampButton->SetVisibility(bBuilding ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SpeedNote->SetVisibility(bInWave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (bBuilding)
	{
		// The countdown never uses the glitch font, so it is always readable.
		LampText->SetText(FText::Format(
			NSLOCTEXT("NightlightCrayon", "LightsOut", "LIGHTS OUT in {0}"),
			FText::AsNumber(FMath::CeilToInt(Director->GetBuildTimeRemaining()))));
	}
}

void UNightlightCrayonHUDWidget::UpdateShelf()
{
	const bool bSlotsFull = Core.IsValid() && !Core->CanPlaceDefender();
	const int32 SelectedCard = GetSelectedCard();
	const bool bCorrupted = SanityState == ENightlightSanityState::LosingIt;

	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		const int32 Cost = GetShelfCost(Index);
		const bool bCore = Index == CoreCardIndex;
		const bool bPicked = bCore ? (Notebook->IsVisible() && NotebookTarget.Get() == Core.Get()) : SelectedCard == Index;
		Cards[Index]->SetCost(Cost);
		Cards[Index]->SetCardState(Cost >= 0 && CanAfford(Cost), bPicked, !bCore && bSlotsFull, bCorrupted);
	}

	SlotsFullHint->SetVisibility(bSlotsFull ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UNightlightCrayonHUDWidget::UpdateMarkers()
{
	APlayerController* const PlayerController = GetOwningPlayer();
	const bool bBuilding = Director.IsValid() && Director->GetWavePhase() == ENightlightWavePhase::BuildPhase;

	// Platforms are generated with the world, so they are looked for again every few seconds.
	PlatformSearchTimer -= GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	if (PlatformSearchTimer <= 0.0f)
	{
		PlatformSearchTimer = 3.0f;
		Platforms.Reset();
		if (UClass* const PlatformClass = GetPlatformClass())
		{
			TArray<AActor*> Found;
			UGameplayStatics::GetAllActorsOfClass(this, PlatformClass, Found);
			for (AActor* const Platform : Found)
			{
				Platforms.Add(Platform);
			}
		}
	}

	// Yellow X marks with "here!" on every free platform during the build phase.
	int32 MarkIndex = 0;
	if (bBuilding)
	{
		for (const TWeakObjectPtr<AActor>& PlatformPtr : Platforms)
		{
			AActor* const Platform = PlatformPtr.Get();
			bool bOccupied = false;
			FVector2D Position;
			if (!Platform || (NightlightBlueprintBridge::ReadBool(Platform, TEXT("IsOccupied"), bOccupied) && bOccupied)
				|| !ProjectToHUD(Platform->GetActorLocation(), Position) || MarkIndex >= MaxPlatformMarks)
			{
				continue;
			}

			if (!PlatformMarks.IsValidIndex(MarkIndex))
			{
				UVerticalBox* const Mark = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
				if (UVerticalBoxSlot* const XSlot = Mark->AddChildToVerticalBox(
					MakeImage(NightlightCrayonStyle::LoadTexture(TEXT("Overlays/T_Placement_X")), FVector2D(44.0f))))
				{
					XSlot->SetHorizontalAlignment(HAlign_Center);
				}
				UTextBlock* const Here = MakeText(NSLOCTEXT("NightlightCrayon", "Here", "here!"), ENightlightCrayonFont::Kid, 20, NightlightCrayonStyle::Sun());
				Here->SetShadowOffset(FVector2D(1.0f, 1.0f));
				Here->SetShadowColorAndOpacity(NightlightCrayonStyle::Ink());
				if (UVerticalBoxSlot* const HereSlot = Mark->AddChildToVerticalBox(Here))
				{
					HereSlot->SetHorizontalAlignment(HAlign_Center);
				}
				if (UCanvasPanelSlot* const MarkSlot = MarkerLayer->AddChildToCanvas(Mark))
				{
					MarkSlot->SetAlignment(FVector2D(0.5f, 0.7f));
					MarkSlot->SetAutoSize(true);
				}
				PlatformMarks.Add(Mark);
			}

			UWidget* const Mark = PlatformMarks[MarkIndex];
			Mark->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* const MarkSlot = Cast<UCanvasPanelSlot>(Mark->Slot))
			{
				MarkSlot->SetPosition(FVector2D(FMath::RoundToFloat(Position.X), FMath::RoundToFloat(Position.Y)));
			}
			++MarkIndex;
		}
	}

	for (int32 Index = MarkIndex; Index < PlatformMarks.Num(); ++Index)
	{
		PlatformMarks[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}

	// The range ring follows the hovered platform while a defender is picked: yellow where it can go,
	// red where placing is blocked.
	RangeRing->SetVisibility(ESlateVisibility::Collapsed);
	const int32 SelectedCard = GetSelectedCard();
	AActor* const Hovered = GetActorUnderCursor();
	UClass* const PlatformClass = GetPlatformClass();
	bHoveringPlatform = Hovered && PlatformClass && Hovered->IsA(PlatformClass);
	if (bHoveringPlatform && SelectedCard != INDEX_NONE && PlayerController)
	{
		UClass* const DefenderClass = ShelfEntries[SelectedCard].DefenderClassPath.TryLoadClass<AActor>();
		const ANightlightDefender* const DefenderDefaults = DefenderClass ? Cast<ANightlightDefender>(DefenderClass->GetDefaultObject()) : nullptr;
		if (DefenderDefaults)
		{
			bool bOccupied = false;
			NightlightBlueprintBridge::ReadBool(Hovered, TEXT("IsOccupied"), bOccupied);
			const bool bBlocked = bOccupied || !CanAfford(GetShelfCost(SelectedCard)) || (Core.IsValid() && !Core->CanPlaceDefender());
			RangeRing->SetBrushResourceObject(NightlightCrayonStyle::LoadTexture(bBlocked ? TEXT("Overlays/T_Range_Ring_Red") : TEXT("Overlays/T_Range_Ring_Yellow")));
			PlaceRing(RangeRing, Hovered->GetActorLocation(), DefenderDefaults->GetAttackRange());
		}
	}

	UpdateNextMarker(SelectedCard, Hovered);

	// The blue ring and the crayon arrow point out what the notebook page is about.
	SelectedRing->SetVisibility(ESlateVisibility::Collapsed);
	NotebookArrow->SetVisibility(ESlateVisibility::Collapsed);
	AActor* const Target = Notebook->IsVisible() ? NotebookTarget.Get() : nullptr;
	if (!Target)
	{
		return;
	}

	const ANightlightDefender* const Defender = Cast<ANightlightDefender>(Target);
	const float Range = Defender ? Defender->GetAttackRange() : (Core.Get() == Target ? Core->GetAttackRange() : 0.0f);
	if (Range > 0.0f)
	{
		PlaceRing(SelectedRing, Target->GetActorLocation(), Range);
	}

	FVector2D TargetPosition;
	if (ProjectToHUD(Target->GetActorLocation(), TargetPosition))
	{
		const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
		const FVector2D PageEdge(ViewportSize.X - 30.0f - 460.0f - 10.0f, ViewportSize.Y * 0.5f - 20.0f);
		const FVector2D Direction = (TargetPosition - PageEdge).GetSafeNormal();
		if (!Direction.IsNearlyZero() && FVector2D::Distance(TargetPosition, PageEdge) > 140.0f)
		{
			NotebookArrow->SetVisibility(ESlateVisibility::HitTestInvisible);
			NotebookArrow->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)));
			if (UCanvasPanelSlot* const ArrowSlot = Cast<UCanvasPanelSlot>(NotebookArrow->Slot))
			{
				ArrowSlot->SetPosition(PageEdge + Direction * 90.0f);
			}
		}
	}
}

void UNightlightCrayonHUDWidget::UpdateNextMarker(const int32 SelectedCard, AActor* const Hovered)
{
	if (SelectedCard == INDEX_NONE || GameOverScreen)
	{
		NextMarker->SetVisibility(ESlateVisibility::Collapsed);
		ShownNextCard = INDEX_NONE;
		return;
	}

	const bool bCorrupted = SanityState == ENightlightSanityState::LosingIt;
	if (SelectedCard != ShownNextCard || bCorrupted != bShownNextCorrupted)
	{
		ShownNextCard = SelectedCard;
		bShownNextCorrupted = bCorrupted;
		const FShelfEntry& Entry = ShelfEntries[SelectedCard];
		NextDrawing->SetFrames(NightlightCrayonStyle::LoadBoilFrames(Entry.DrawingPath), FVector2D(90.0f, 72.0f));
		NextText->SetFont(NightlightCrayonStyle::GetLabelFont(bCorrupted, 24));
		NextText->SetText(FText::Format(
			NSLOCTEXT("NightlightCrayon", "NextDefender", "next: {0}"),
			bCorrupted ? UNightlightSanityLibrary::CorruptText(Entry.Name) : Entry.Name));
	}

	// Over a platform the drawing sits on top of it, so it previews where the defender will stand.
	// Anywhere else it trails just above and right of the mouse.
	FVector2D Position = FVector2D::ZeroVector;
	UClass* const PlatformClass = GetPlatformClass();
	const bool bOnPlatform = Hovered && PlatformClass && Hovered->IsA(PlatformClass) && ProjectToHUD(Hovered->GetActorLocation(), Position);
	if (!bOnPlatform)
	{
		Position = UWidgetLayoutLibrary::GetMousePositionOnViewport(this) + FVector2D(70.0f, -12.0f);
	}

	NextMarker->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* const NextSlot = Cast<UCanvasPanelSlot>(NextMarker->Slot))
	{
		NextSlot->SetPosition(FVector2D(FMath::RoundToFloat(Position.X), FMath::RoundToFloat(Position.Y - (bOnPlatform ? 20.0f : 0.0f))));
	}
}

void UNightlightCrayonHUDWidget::UpdateNotebook()
{
	if (!Notebook->IsVisible())
	{
		return;
	}

	AActor* const Target = NotebookTarget.Get();
	const ANightlightDefender* const Defender = Cast<ANightlightDefender>(Target);
	if (!Target || (Defender && Defender->IsDead()))
	{
		CloseNotebook();
		return;
	}

	Notebook->SetInfo(MakeNotebookInfo(Target));
}

FNightlightNotebookInfo UNightlightCrayonHUDWidget::MakeNotebookInfo(AActor* const Target) const
{
	FNightlightNotebookInfo Info;
	if (ANightlightDreamCore* const TargetCore = Cast<ANightlightDreamCore>(Target))
	{
		Info.Title = NSLOCTEXT("NightlightCrayon", "CoreTitle", "my dream heart");
		Info.PortraitPath = ShelfEntries[CoreCardIndex].DrawingPath;
		Info.Level = TargetCore->GetUpgradeLevel();
		Info.MaxLevel = TargetCore->GetMaxUpgradeLevel();
		Info.Cost = TargetCore->GetNextUpgradeCost();

		// The Core's tiers are known in advance, so every line can say what the next star adds.
		FNightlightUpgradeTier Next;
		const bool bHasNext = TargetCore->GetNextUpgradeTier(Next);
		Info.StatLines.Add(StatLine(NSLOCTEXT("NightlightCrayon", "StatHealth", "health"), TargetCore->GetMaxHealth(),
			bHasNext ? TOptional<float>(TargetCore->GetMaxHealth() + Next.MaxHealthBonus) : TOptional<float>()));
		Info.StatLines.Add(StatLine(NSLOCTEXT("NightlightCrayon", "StatHelpers", "friends"), TargetCore->GetMaxDefenders(),
			bHasNext ? TOptional<float>(TargetCore->GetMaxDefenders() + Next.ExtraDefenderSlots) : TOptional<float>()));
		if (bHasNext && Next.AttackDamageBonus > 0.0f)
		{
			Info.StatLines.Add(FText::Format(NSLOCTEXT("NightlightCrayon", "StatPowerBonus", "power: +{0}!"), FText::AsNumber(FMath::RoundToInt(Next.AttackDamageBonus))));
		}
	}
	else if (Target)
	{
		// Shooter and Torch are children of the Patroller Blueprint, so the most specific match wins.
		Info.Title = NSLOCTEXT("NightlightCrayon", "DefenderTitle", "my friend");
		for (int32 Index = CoreCardIndex - 1; Index >= 0; --Index)
		{
			UClass* const EntryClass = ShelfEntries[Index].DefenderClassPath.TryLoadClass<AActor>();
			if (EntryClass && Target->IsA(EntryClass))
			{
				Info.Title = ShelfEntries[Index].Name;
				Info.PortraitPath = ShelfEntries[Index].DrawingPath;
				break;
			}
		}

		// BPI_Upgradeable gives the level and cost. The max level and the per tier changes are read from
		// the defender's own variables when it has them.
		NightlightBlueprintBridge::CallFunction(Target, TEXT("GetUpgradeLevel"), nullptr, &Info.Level);
		Info.MaxLevel = 2;
		NightlightBlueprintBridge::ReadInt(Target, TEXT("MaxUpgradeLevel"), Info.MaxLevel);
		int32 Cost = -1;
		NightlightBlueprintBridge::CallFunction(Target, TEXT("GetUpgradeCost"), nullptr, &Cost);
		const bool bHasNext = Info.Level < Info.MaxLevel && Cost >= 0;
		Info.Cost = bHasNext ? Cost : -1;

		float Damage = 0.0f;
		float DamageMultiplier = 0.0f;
		float HealthPerUpgrade = 0.0f;
		NightlightBlueprintBridge::ReadFloat(Target, TEXT("AttackDamage"), Damage);
		const bool bHasMultiplier = NightlightBlueprintBridge::ReadFloat(Target, TEXT("DamageMultiplier"), DamageMultiplier) && DamageMultiplier > 0.0f;
		NightlightBlueprintBridge::ReadFloat(Target, TEXT("HealthPerUpgrade"), HealthPerUpgrade);

		Info.StatLines.Add(StatLine(NSLOCTEXT("NightlightCrayon", "StatPower", "power"), Damage,
			bHasNext && bHasMultiplier ? TOptional<float>(Damage * DamageMultiplier) : TOptional<float>()));
		if (const ANightlightDefender* const Defender = Cast<ANightlightDefender>(Target))
		{
			Info.StatLines.Add(StatLine(NSLOCTEXT("NightlightCrayon", "StatHealth", "health"), Defender->GetMaxHealth(),
				bHasNext && HealthPerUpgrade > 0.0f ? TOptional<float>(Defender->GetMaxHealth() + HealthPerUpgrade) : TOptional<float>()));
			Info.StatLines.Add(StatLine(NSLOCTEXT("NightlightCrayon", "StatReach", "reach"), Defender->GetAttackRange() / 100.0f, TOptional<float>()));
		}
	}

	Info.bCanAfford = Info.Cost >= 0 && CanAfford(Info.Cost);
	return Info;
}

void UNightlightCrayonHUDWidget::SweepOldWidgets()
{
	// The player controller still creates the old widgets, so they are swapped for the crayon ones as
	// soon as they appear (Epic Games, Inc., 2026d).
	TArray<UUserWidget*> TopLevelWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, TopLevelWidgets, UUserWidget::StaticClass(), true);
	for (UUserWidget* const Widget : TopLevelWidgets)
	{
		const FString ClassName = Widget->GetClass()->GetName();
		if (ClassName.StartsWith(TEXT("WBP_GameHUD")))
		{
			Widget->RemoveFromParent();
		}
		else if (ClassName.StartsWith(TEXT("WBP_PaueMenu")))
		{
			Widget->RemoveFromParent();
			ShowPauseScreen();
		}
		else if (ClassName.StartsWith(TEXT("WBP_GameOver")))
		{
			Widget->RemoveFromParent();
			ShowGameOver();
		}
	}

	// The controller's own pause key unpauses without knowing about the crayon screen.
	if (PauseScreen && PauseScreen->IsInViewport() && !UGameplayStatics::IsGamePaused(this))
	{
		PauseScreen->RemoveFromParent();
	}
}

void UNightlightCrayonHUDWidget::RefreshWaveLabel()
{
	if (!WaveLabel)
	{
		return;
	}

	FText Label = NSLOCTEXT("NightlightCrayon", "WaveQuiet", "the night is quiet...");
	if (Director.IsValid())
	{
		const ENightlightWavePhase Phase = Director->GetWavePhase();
		if (Phase == ENightlightWavePhase::BuildPhase)
		{
			Label = FText::Format(NSLOCTEXT("NightlightCrayon", "WaveComing", "NIGHT {0} is coming..."), FText::AsNumber(Director->GetCurrentWaveNumber() + 1));
		}
		else if (Phase != ENightlightWavePhase::NotStarted)
		{
			Label = FText::Format(
				NSLOCTEXT("NightlightCrayon", "WaveLeft", "NIGHT {0} Â· {1} LEFT"),
				FText::AsNumber(Director->GetCurrentWaveNumber()), FText::AsNumber(EnemiesLeft));
		}
	}

	const bool bCorrupted = SanityState == ENightlightSanityState::LosingIt;
	WaveLabel->SetText(Label);
	WaveLabel->SetFont(NightlightCrayonStyle::GetFont(bCorrupted ? ENightlightCrayonFont::Corrupted : ENightlightCrayonFont::Nightmare, 34));
}

void UNightlightCrayonHUDWidget::SetGameAndUIInput() const
{
	if (APlayerController* const PlayerController = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(true);
	}
}

void UNightlightCrayonHUDWidget::ShowPauseScreen()
{
	if (GameOverScreen)
	{
		return;
	}

	if (!PauseScreen)
	{
		PauseScreen = CreateWidget<UNightlightCrayonScreen>(GetOwningPlayer(), UNightlightCrayonScreen::StaticClass());
		PauseScreen->SetupPause();
	}

	if (!PauseScreen->IsInViewport())
	{
		PauseScreen->AddToViewport(50);
	}

	SetGameAndUIInput();
}

void UNightlightCrayonHUDWidget::ShowGameOver()
{
	if (GameOverScreen)
	{
		return;
	}

	UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	if (PauseScreen)
	{
		PauseScreen->RemoveFromParent();
	}

	GameOverScreen = CreateWidget<UNightlightCrayonScreen>(GetOwningPlayer(), UNightlightCrayonScreen::StaticClass());
	GameOverScreen->SetupGameOver(Director.IsValid() ? FMath::Max(Director->GetCurrentWaveNumber() - 1, 0) : 0);
	GameOverScreen->AddToViewport(60);
	SetGameAndUIInput();
}

void UNightlightCrayonHUDWidget::StartWaveEarly()
{
	if (Director.IsValid() && Director->GetWavePhase() == ENightlightWavePhase::BuildPhase)
	{
		Director->StartNextWaveNow();
	}
}

void UNightlightCrayonHUDWidget::ToggleSpeed()
{
	if (GameOverScreen)
	{
		return;
	}

	// Time dilation speeds up the whole world, timers included (Epic Games, Inc., 2026a).
	bFastForward = !bFastForward;
	UGameplayStatics::SetGlobalTimeDilation(this, bFastForward ? 2.0f : 1.0f);
	SpeedText->SetText(bFastForward
		? NSLOCTEXT("NightlightCrayon", "Slower", "slow down! (F)")
		: NSLOCTEXT("NightlightCrayon", "Faster", "FASTER!! (F)"));
}

void UNightlightCrayonHUDWidget::TogglePause()
{
	if (PauseScreen && PauseScreen->IsInViewport())
	{
		PauseScreen->Resume();
		return;
	}

	if (GameOverScreen)
	{
		return;
	}

	UGameplayStatics::SetGamePaused(this, true);
	NightlightBlueprintBridge::WriteBool(GetOwningPlayer(), TEXT("IsPaused"), true);
	ShowPauseScreen();
}

void UNightlightCrayonHUDWidget::ToggleNotebookUnderCursor()
{
	if (Notebook->IsVisible())
	{
		CloseNotebook();
		return;
	}

	if (AActor* const Target = FindUpgradeable(GetActorUnderCursor()))
	{
		OpenNotebook(Target);
	}
}

void UNightlightCrayonHUDWidget::SelectUnderCursor()
{
	AActor* const Hit = GetActorUnderCursor();
	if (AActor* const Target = FindUpgradeable(Hit))
	{
		OpenNotebook(Target);
		return;
	}

	// Clicking an empty spot closes the page, but placing on a platform leaves it alone.
	UClass* const PlatformClass = GetPlatformClass();
	if (!Hit || !PlatformClass || !Hit->IsA(PlatformClass))
	{
		CloseNotebook();
	}
}

void UNightlightCrayonHUDWidget::OpenNotebook(AActor* const Target)
{
	if (!Target)
	{
		return;
	}

	NotebookTarget = Target;
	Notebook->SetVisibility(ESlateVisibility::Visible);
	Notebook->SetInfo(MakeNotebookInfo(Target));
	NotebookRefreshTimer = 0.5f;
}

void UNightlightCrayonHUDWidget::CloseNotebook()
{
	NotebookTarget.Reset();
	Notebook->SetVisibility(ESlateVisibility::Collapsed);
}

AGameStateBase* UNightlightCrayonHUDWidget::GetGameState() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetGameState() : nullptr;
}

AActor* UNightlightCrayonHUDWidget::GetActorUnderCursor() const
{
	APlayerController* const PlayerController = GetOwningPlayer();
	FHitResult Hit;
	if (!PlayerController || !PlayerController->GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
	{
		return nullptr;
	}

	return Hit.GetActor();
}

AActor* UNightlightCrayonHUDWidget::FindUpgradeable(AActor* const HitActor) const
{
	if (!HitActor)
	{
		return nullptr;
	}

	if (HitActor->IsA<ANightlightDreamCore>())
	{
		return HitActor;
	}

	auto IsUpgradeable = [](const AActor* const Actor)
	{
		return Actor && NightlightBlueprintBridge::HasFunction(Actor, TEXT("Upgrade"))
			&& NightlightBlueprintBridge::HasFunction(Actor, TEXT("GetUpgradeCost"));
	};

	if (IsUpgradeable(HitActor))
	{
		return HitActor;
	}

	// Clicking a platform opens the page for the defender standing on it.
	if (AActor* const Placed = Cast<AActor>(NightlightBlueprintBridge::ReadObject(HitActor, TEXT("PlacedDefender"))); IsUpgradeable(Placed))
	{
		return Placed;
	}

	AActor* const Owner = HitActor->GetOwner();
	return IsUpgradeable(Owner) ? Owner : nullptr;
}

bool UNightlightCrayonHUDWidget::CanAfford(const int32 Cost) const
{
	return Tokens != INDEX_NONE && Cost >= 0 && Tokens >= Cost;
}

bool UNightlightCrayonHUDWidget::SpendTokens(const int32 Cost) const
{
	bool bSpent = false;
	int32 Amount = Cost;
	return NightlightBlueprintBridge::CallFunction(GetGameState(), TEXT("TrySpendDreamTokens"), &Amount, nullptr, &bSpent) && bSpent;
}

int32 UNightlightCrayonHUDWidget::GetSelectedCard() const
{
	// The controller's Selected Defender is an index (0 Patroller, 1 Shooter, 2 Torch), and -1 means none
	// is picked. It wins when it can be read, so the shelf always matches what a click will place.
	int32 Selected = INDEX_NONE;
	if (NightlightBlueprintBridge::ReadInt(GetOwningPlayer(), TEXT("SelectedDefender"), Selected))
	{
		return Selected >= 0 && Selected < CoreCardIndex ? Selected : INDEX_NONE;
	}

	return PickedCard;
}

bool UNightlightCrayonHUDWidget::PickCard(const int32 CardIndex)
{
	const int32 NewCard = CardIndex >= 0 && CardIndex < CoreCardIndex ? CardIndex : INDEX_NONE;

	// Picking the card that is already picked puts it back on the shelf.
	if (NewCard == INDEX_NONE || NewCard == GetSelectedCard())
	{
		ClearPickedCard();
		return false;
	}

	PickedCard = NewCard;
	return true;
}

void UNightlightCrayonHUDWidget::ClearPickedCard()
{
	PickedCard = INDEX_NONE;

	// The controller's own 1 to 3 handler runs after the HUD's, so it would pick the card straight back.
	// Its Selected Defender is cleared now and again on the next tick, after that handler has run.
	NightlightBlueprintBridge::WriteInt(GetOwningPlayer(), TEXT("SelectedDefender"), INDEX_NONE);
	bClearControllerSelection = true;
}

bool UNightlightCrayonHUDWidget::ShouldBlockPlacementClick() const
{
	// With nothing picked, a click on a free platform is held back, so the controller cannot place its
	// default Patroller. Clicks anywhere else still reach it.
	return bHoveringPlatform && GetSelectedCard() == INDEX_NONE && !GameOverScreen;
}

int32 UNightlightCrayonHUDWidget::GetShelfCost(const int32 CardIndex) const
{
	if (CardIndex == CoreCardIndex)
	{
		return Core.IsValid() ? Core->GetNextUpgradeCost() : -1;
	}

	int32 Cost = -1;
	if (ShelfEntries.IsValidIndex(CardIndex))
	{
		NightlightBlueprintBridge::ReadInt(GetOwningPlayer(), ShelfEntries[CardIndex].CostProperty, Cost);
	}

	return Cost;
}

bool UNightlightCrayonHUDWidget::ProjectToHUD(const FVector& WorldLocation, FVector2D& OutPosition) const
{
	// Returns the position in this widget's DPI scaled units, so it lines up with the canvas slots.
	APlayerController* const PlayerController = GetOwningPlayer();
	if (!PlayerController || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController, WorldLocation, OutPosition, true))
	{
		return false;
	}

	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	return OutPosition.X >= -100.0f && OutPosition.Y >= -100.0f && OutPosition.X <= ViewportSize.X + 100.0f && OutPosition.Y <= ViewportSize.Y + 100.0f;
}

float UNightlightCrayonHUDWidget::ProjectRadius(const FVector& WorldLocation, const float WorldRadius) const
{
	const APlayerController* const PlayerController = GetOwningPlayer();
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return 0.0f;
	}

	// Projecting a point one radius to the side gives the ring's size on screen at that distance.
	const FVector Right = FRotationMatrix(PlayerController->PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Y);
	FVector2D Centre;
	FVector2D Edge;
	if (!ProjectToHUD(WorldLocation, Centre) || !ProjectToHUD(WorldLocation + Right * WorldRadius, Edge))
	{
		return 0.0f;
	}

	return FVector2D::Distance(Centre, Edge);
}

void UNightlightCrayonHUDWidget::PlaceRing(UImage* const Ring, const FVector& WorldLocation, const float WorldRadius)
{
	FVector2D Centre;
	const float Radius = ProjectRadius(WorldLocation, WorldRadius);
	if (Radius <= 4.0f || !ProjectToHUD(WorldLocation, Centre))
	{
		return;
	}

	Ring->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* const RingSlot = Cast<UCanvasPanelSlot>(Ring->Slot))
	{
		RingSlot->SetAutoSize(false);
		RingSlot->SetPosition(Centre);
		RingSlot->SetSize(FVector2D(Radius * 2.0f));
	}
}

void UNightlightCrayonHUDWidget::HandleCoreHealthChanged(const float CurrentHealth, const float MaxHealth)
{
	TargetSanity = MaxHealth > 0.0f ? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f) : 0.0f;
	NotebookRefreshTimer = 0.0f;
}

void UNightlightCrayonHUDWidget::HandleCoreDestroyed()
{
	TargetSanity = 0.0f;
	ShowGameOver();
}

void UNightlightCrayonHUDWidget::HandleBuildPhaseStarted(const int32 NextWaveNumber, const float BuildSeconds)
{
	(void)NextWaveNumber;
	(void)BuildSeconds;
	RefreshWaveLabel();
}

void UNightlightCrayonHUDWidget::HandleWaveStarted(const FNightlightWaveInfo& Info)
{
	bWarnedThisWave = false;
	EnemiesLeft = Info.EnemyCount;
	RefreshWaveLabel();
	if (Info.bIsPeakWave)
	{
		Warning->ShowMessage(NSLOCTEXT("NightlightCrayon", "PeakWave", "THE BIG NIGHTMARE IS HERE"), 2.5f, 44);
	}
}

void UNightlightCrayonHUDWidget::HandleEnemiesRemainingChanged(const int32 EnemiesRemaining)
{
	EnemiesLeft = EnemiesRemaining;
	RefreshWaveLabel();
}

void UNightlightCrayonHUDWidget::HandleWaveCleared(const FNightlightWaveSummary& Summary)
{
	WaveClearedSummary->SetText(FText::Format(
		NSLOCTEXT("NightlightCrayon", "WaveSummary", "night {0} done! stopped {1} of {2} nightmares"),
		FText::AsNumber(Summary.WaveNumber), FText::AsNumber(Summary.EnemiesStopped), FText::AsNumber(Summary.EnemiesSpawned)));
	WaveCleared->SetVisibility(ESlateVisibility::HitTestInvisible);
	WaveCleared->SetRenderOpacity(1.0f);
	WaveClearedTimeLeft = 3.0f;
	RefreshWaveLabel();
}

void UNightlightCrayonHUDWidget::HandleEnemySpawned(ANightlightEnemy* const Enemy)
{
	const bool bBrute = Enemy && Enemy->IsA<ANightlightEnemyBrute>();
	const bool bShade = Enemy && Enemy->IsA<ANightlightEnemyShade>();
	if ((!bBrute && !bShade) || Warning->IsShowing())
	{
		return;
	}

	const APlayerController* const PlayerController = GetOwningPlayer();
	const FRotator CameraRotation = PlayerController && PlayerController->PlayerCameraManager
		? PlayerController->PlayerCameraManager->GetCameraRotation() : FRotator::ZeroRotator;
	const FVector From = Enemy->GetActorLocation() - (Core.IsValid() ? Core->GetActorLocation() : FVector::ZeroVector);
	const FText Direction = DescribeDirection(From, CameraRotation);
	Warning->ShowMessage(
		bBrute
			? FText::Format(NSLOCTEXT("NightlightCrayon", "BruteWarning", "SOMETHING BIG IS COMING FROM THE {0}"), Direction)
			: FText::Format(NSLOCTEXT("NightlightCrayon", "ShadeWarning", "SOMETHING IS WATCHING FROM THE {0}"), Direction),
		2.0f, 40);
}

void UNightlightCrayonHUDWidget::HandleCardClicked(const int32 CardIndex)
{
	if (CardIndex == CoreCardIndex)
	{
		if (Notebook->IsVisible() && NotebookTarget.Get() == Core.Get())
		{
			CloseNotebook();
		}
		else
		{
			OpenNotebook(Core.Get());
		}
		return;
	}

	if (!PickCard(CardIndex))
	{
		return;
	}

	// The same variable keys 1 to 3 set, so clicking a card starts the controller's usual placement.
	if (!NightlightBlueprintBridge::WriteInt(GetOwningPlayer(), TEXT("SelectedDefender"), CardIndex) && !bWarnedAboutSelection)
	{
		bWarnedAboutSelection = true;
		UE_LOG(LogTemp, Warning, TEXT("Crayon HUD could not set Selected Defender on the player controller."));
	}
}

void UNightlightCrayonHUDWidget::HandleUpgradeClicked()
{
	AActor* const Target = NotebookTarget.Get();
	if (!Target)
	{
		return;
	}

	// Same order as the U key: check the cost, spend the tokens, then apply the upgrade.
	if (ANightlightDreamCore* const TargetCore = Cast<ANightlightDreamCore>(Target))
	{
		const int32 Cost = TargetCore->GetNextUpgradeCost();
		if (Cost >= 0 && TargetCore->CanUpgradeCore() && CanAfford(Cost) && SpendTokens(Cost))
		{
			TargetCore->UpgradeCore();
		}
	}
	else
	{
		const FNightlightNotebookInfo Info = MakeNotebookInfo(Target);
		if (Info.Cost >= 0 && Info.bCanAfford && SpendTokens(Info.Cost))
		{
			NightlightBlueprintBridge::CallFunction(Target, TEXT("Upgrade"));
		}
	}

	UpdateTokens();
	Notebook->SetInfo(MakeNotebookInfo(Target));
}

void UNightlightCrayonHUDWidget::HandleLampClicked()
{
	StartWaveEarly();
}

void UNightlightCrayonHUDWidget::HandleSpeedClicked()
{
	ToggleSpeed();
}

/*
References

Epic Games, Inc., 2026a. UGameplayStatics::SetGlobalTimeDilation. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Kismet/UGameplayStatics/SetGlobalTimeDilation>
[Accessed 3 November 2026].

Epic Games, Inc., 2026b. UGameplayStatics::GetActorOfClass. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Kismet/UGameplayStatics/GetActorOfClass>
[Accessed 3 November 2026].

Epic Games, Inc., 2026c. Retainer Box. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/retainer-box-in-unreal-engine>
[Accessed 3 November 2026].

Epic Games, Inc., 2026d. UWidgetBlueprintLibrary::GetAllWidgetsOfClass. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/Blueprint/UWidgetBlueprintLibrary/GetAllWidgetsOfClass>
[Accessed 3 November 2026].
*/
