#include "NightlightCrayonScreens.h"
#include "NightlightBlueprintBridge.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UNightlightCrayonScreen::BuildTree()
{
	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	// A brush with no texture draws as a flat colour, here a sheet of ink over the game.
	Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Backdrop->SetColorAndOpacity(NightlightCrayonStyle::Ink().CopyWithNewOpacity(0.7f));
	if (UOverlaySlot* const BackdropSlot = Root->AddChildToOverlay(Backdrop))
	{
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UNightlightPaperPanel* const Panel = CreateWidget<UNightlightPaperPanel>(this, UNightlightPaperPanel::StaticClass());
	Panel->Setup(TEXT("Panels/T_Paper_Scrap"), 0.1f, FMargin(48.0f, 30.0f), true, 2.5f);
	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);
	if (UOverlaySlot* const PanelSlot = Root->AddChildToOverlay(Panel))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}
}

UNightlightCrayonButton* UNightlightCrayonScreen::AddButton(const ENightlightCrayonButtonKind Kind, const FText& Label)
{
	UNightlightCrayonButton* const Button = CreateWidget<UNightlightCrayonButton>(this, UNightlightCrayonButton::StaticClass());
	Button->Setup(Kind, Label, 28);
	if (UVerticalBoxSlot* const ButtonSlot = Column->AddChildToVerticalBox(Button))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSlot->SetPadding(FMargin(0.0f, 6.0f));
	}

	return Button;
}

void UNightlightCrayonScreen::SetupPause()
{
	EnsureBuilt();
	Column->ClearChildren();

	UTextBlock* const Title = MakeText(NSLOCTEXT("NightlightCrayon", "PauseTitle", "shh... paused"), ENightlightCrayonFont::Kid, 52, NightlightCrayonStyle::Sky());
	if (UVerticalBoxSlot* const TitleSlot = Column->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	AddButton(ENightlightCrayonButtonKind::Buy, NSLOCTEXT("NightlightCrayon", "PauseResume", "keep going"))
		->OnClicked.AddDynamic(this, &UNightlightCrayonScreen::Resume);
	AddButton(ENightlightCrayonButtonKind::Secondary, NSLOCTEXT("NightlightCrayon", "PauseRestart", "start again"))
		->OnClicked.AddDynamic(this, &UNightlightCrayonScreen::HandleRestart);
	AddButton(ENightlightCrayonButtonKind::Secondary, NSLOCTEXT("NightlightCrayon", "PauseQuit", "wake up (quit)"))
		->OnClicked.AddDynamic(this, &UNightlightCrayonScreen::HandleQuit);
}

void UNightlightCrayonScreen::SetupGameOver(const int32 NightsSurvived)
{
	EnsureBuilt();
	Column->ClearChildren();
	Backdrop->SetColorAndOpacity(NightlightCrayonStyle::Ink().CopyWithNewOpacity(0.92f));

	UTextBlock* const Title = MakeText(NSLOCTEXT("NightlightCrayon", "GameOverTitle", "IT GOT IN..."), ENightlightCrayonFont::Nightmare, 64, NightlightCrayonStyle::NightmareRed());
	if (UVerticalBoxSlot* const TitleSlot = Column->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UTextBlock* const Summary = MakeText(
		FText::Format(NSLOCTEXT("NightlightCrayon", "GameOverSummary", "you kept the lights on for {0} {0}|plural(one=night,other=nights)"), FText::AsNumber(NightsSurvived)),
		ENightlightCrayonFont::Kid, 30, NightlightCrayonStyle::Pencil());
	if (UVerticalBoxSlot* const SummarySlot = Column->AddChildToVerticalBox(Summary))
	{
		SummarySlot->SetHorizontalAlignment(HAlign_Center);
		SummarySlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 14.0f));
	}

	AddButton(ENightlightCrayonButtonKind::Buy, NSLOCTEXT("NightlightCrayon", "GameOverRestart", "try again"))
		->OnClicked.AddDynamic(this, &UNightlightCrayonScreen::HandleRestart);
	AddButton(ENightlightCrayonButtonKind::Secondary, NSLOCTEXT("NightlightCrayon", "GameOverQuit", "wake up (quit)"))
		->OnClicked.AddDynamic(this, &UNightlightCrayonScreen::HandleQuit);
}

void UNightlightCrayonScreen::Resume()
{
	UGameplayStatics::SetGamePaused(this, false);
	if (APlayerController* const PlayerController = GetOwningPlayer())
	{
		NightlightBlueprintBridge::WriteBool(PlayerController, TEXT("IsPaused"), false);

		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(true);
	}

	RemoveFromParent();
}

void UNightlightCrayonScreen::HandleRestart()
{
	// Same as the old menus: reload the level that is open now (Epic Games, Inc., 2026).
	UGameplayStatics::OpenLevel(this, FName(UGameplayStatics::GetCurrentLevelName(this)));
}

void UNightlightCrayonScreen::HandleQuit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

/*
References

Epic Games, Inc., 2026. UGameplayStatics::OpenLevel. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Kismet/UGameplayStatics/OpenLevel>
[Accessed 3 November 2026].
*/
