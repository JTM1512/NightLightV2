#include "NightlightHealthWidgetUtils.h"
#include "NightlightCrayonHealthBar.h"
#include "NightlightCrayonHUD.h"
#include "../Enemies/NightlightEnemy.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/World.h"

namespace NightlightHealthWidgetUtils
{
	namespace
	{
		// How many centimetres higher the crayon bar floats than the old bar did.
		constexpr float CrayonBarExtraHeight = 80.0f;

		bool UsesCrayonHUD(const AActor* const Actor)
		{
			const UWorld* const World = Actor->GetWorld();
			const AGameModeBase* const GameMode = World ? World->GetAuthGameMode() : nullptr;
			return GameMode && GameMode->HUDClass && GameMode->HUDClass->IsChildOf(ANightlightCrayonHUD::StaticClass());
		}
	}

	UUserWidget* GetWorldHealthWidget(AActor* const Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		UWidgetComponent* const WidgetComponent = Actor->FindComponentByClass<UWidgetComponent>();
		if (!WidgetComponent)
		{
			return nullptr;
		}

		// While the game mode uses the crayon HUD, the old WBP_WorldHealthBar is swapped for the crayon bar
		// the first time each actor updates it, so no defender or enemy Blueprint has to change.
		UClass* const CurrentClass = WidgetComponent->GetWidgetClass();
		if (CurrentClass && CurrentClass->GetName().StartsWith(TEXT("WBP_WorldHealthBar")) && UsesCrayonHUD(Actor))
		{
			WidgetComponent->SetWidgetClass(UNightlightCrayonHealthBar::StaticClass());

			// The swap only happens once per actor, so the bar is lifted once to float clear of the model.
			WidgetComponent->AddRelativeLocation(FVector(0.0f, 0.0f, CrayonBarExtraHeight));
		}

		// InitWidget creates the widget if the component has not drawn yet, so the first hit
		// can still update it (Epic Games, Inc., 2026b).
		WidgetComponent->InitWidget();
		UUserWidget* const Widget = WidgetComponent->GetUserWidgetObject();
		if (UNightlightCrayonHealthBar* const CrayonBar = Cast<UNightlightCrayonHealthBar>(Widget))
		{
			CrayonBar->SetIsNightmare(Actor->IsA<ANightlightEnemy>());
		}

		return Widget;
	}

	void UpdateWorldHealthWidget(
		AActor* const Actor,
		const double CurrentHealth,
		const double MaxHealth,
		const double DamageTaken)
	{
		UUserWidget* const HealthWidget = GetWorldHealthWidget(Actor);
		if (!HealthWidget)
		{
			return;
		}

		// The widgets are found by name so any Blueprint health bar with these names works
		// (Epic Games, Inc., 2026a).
		if (UProgressBar* const HealthBar = Cast<UProgressBar>(HealthWidget->GetWidgetFromName(TEXT("HealthBar"))))
		{
			const float HealthPercent = MaxHealth > 0.0
				? static_cast<float>(FMath::Clamp(CurrentHealth / MaxHealth, 0.0, 1.0))
				: 0.0f;
			HealthBar->SetPercent(HealthPercent);
		}

		if (UTextBlock* const HealthText = Cast<UTextBlock>(HealthWidget->GetWidgetFromName(TEXT("HealthText"))))
		{
			HealthText->SetText(FText::Format(
				NSLOCTEXT("Nightlight", "WorldHealthFormat", "{0} / {1}"),
				FText::AsNumber(FMath::RoundToInt(CurrentHealth)),
				FText::AsNumber(FMath::RoundToInt(MaxHealth))));
		}

		if (DamageTaken > 0.0)
		{
			// The crayon bar animates its own damage number, so it floats away instead of staying.
			if (UNightlightCrayonHealthBar* const CrayonBar = Cast<UNightlightCrayonHealthBar>(HealthWidget))
			{
				CrayonBar->ShowDamage(static_cast<float>(DamageTaken));
			}
			else if (UTextBlock* const DamageText = Cast<UTextBlock>(HealthWidget->GetWidgetFromName(TEXT("DamageText"))))
			{
				DamageText->SetText(FText::AsNumber(-FMath::RoundToInt(DamageTaken)));
				DamageText->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
}

/*
References

Epic Games, Inc., 2026a. UUserWidget. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UUserWidget>
[Accessed 29 September 2026].

Epic Games, Inc., 2026b. UWidgetComponent. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UWidgetComponent>
[Accessed 29 September 2026].
*/
