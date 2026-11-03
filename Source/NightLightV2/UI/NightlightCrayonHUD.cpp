#include "NightlightCrayonHUD.h"
#include "NightlightCrayonHUDWidget.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

void ANightlightCrayonHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* const PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	UClass* const WidgetClass = HUDWidgetClass ? HUDWidgetClass.Get() : UNightlightCrayonHUDWidget::StaticClass();
	HUDWidget = CreateWidget<UNightlightCrayonHUDWidget>(PlayerController, WidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(1);
	}

	// A pushed input component is checked before the controller's own, so a binding here can let the
	// key through (bConsumeInput false) or keep it for the crayon HUD (Epic Games, Inc., 2026).
	CrayonInput = NewObject<UInputComponent>(this, TEXT("CrayonHUDInput"));
	CrayonInput->Priority = 10;
	CrayonInput->bBlockInput = false;

	auto Bind = [this](const FKey& Key, void (ANightlightCrayonHUD::*Handler)(), const bool bConsume, const bool bWhenPaused)
	{
		FInputKeyBinding& Binding = CrayonInput->BindKey(Key, IE_Pressed, this, Handler);
		Binding.bConsumeInput = bConsume;
		Binding.bExecuteWhenPaused = bWhenPaused;
	};

	Bind(EKeys::SpaceBar, &ANightlightCrayonHUD::HandleStartWave, false, false);
	Bind(EKeys::F, &ANightlightCrayonHUD::HandleSpeed, false, false);
	Bind(EKeys::Escape, &ANightlightCrayonHUD::HandlePause, true, true);

	// U opens the notebook page instead of upgrading straight away, so the old U upgrade is held back.
	Bind(EKeys::U, &ANightlightCrayonHUD::HandleNotebook, true, false);

	// The controller still places defenders with the click, so it is only watched here.
	Bind(EKeys::LeftMouseButton, &ANightlightCrayonHUD::HandleClick, false, false);
	LeftClickBindingIndex = CrayonInput->KeyBindings.Num() - 1;

	// The controller still picks the defender with 1 to 3; the HUD only listens so the card can pop.
	Bind(EKeys::One, &ANightlightCrayonHUD::HandlePickFirst, false, false);
	Bind(EKeys::Two, &ANightlightCrayonHUD::HandlePickSecond, false, false);
	Bind(EKeys::Three, &ANightlightCrayonHUD::HandlePickThird, false, false);

	PlayerController->PushInputComponent(CrayonInput);
}

void ANightlightCrayonHUD::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The click is only kept from the controller while it would place a defender nobody picked.
	if (HUDWidget && CrayonInput && CrayonInput->KeyBindings.IsValidIndex(LeftClickBindingIndex))
	{
		CrayonInput->KeyBindings[LeftClickBindingIndex].bConsumeInput = HUDWidget->ShouldBlockPlacementClick();
	}
}

void ANightlightCrayonHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* const PlayerController = GetOwningPlayerController())
	{
		PlayerController->PopInputComponent(CrayonInput);
	}

	Super::EndPlay(EndPlayReason);
}

void ANightlightCrayonHUD::HandleStartWave()
{
	if (HUDWidget)
	{
		HUDWidget->StartWaveEarly();
	}
}

void ANightlightCrayonHUD::HandleSpeed()
{
	if (HUDWidget)
	{
		HUDWidget->ToggleSpeed();
	}
}

void ANightlightCrayonHUD::HandlePause()
{
	if (HUDWidget)
	{
		HUDWidget->TogglePause();
	}
}

void ANightlightCrayonHUD::HandleNotebook()
{
	if (HUDWidget)
	{
		HUDWidget->ToggleNotebookUnderCursor();
	}
}

void ANightlightCrayonHUD::HandleClick()
{
	if (HUDWidget)
	{
		HUDWidget->SelectUnderCursor();
	}
}

void ANightlightCrayonHUD::HandlePickFirst()
{
	if (HUDWidget)
	{
		HUDWidget->PickCard(0);
	}
}

void ANightlightCrayonHUD::HandlePickSecond()
{
	if (HUDWidget)
	{
		HUDWidget->PickCard(1);
	}
}

void ANightlightCrayonHUD::HandlePickThird()
{
	if (HUDWidget)
	{
		HUDWidget->PickCard(2);
	}
}

/*
References

Epic Games, Inc., 2026. UInputComponent. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Components/UInputComponent>
[Accessed 3 November 2026].
*/
