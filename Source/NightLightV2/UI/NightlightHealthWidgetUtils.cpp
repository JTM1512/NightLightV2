#include "NightlightHealthWidgetUtils.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"

namespace NightlightHealthWidgetUtils
{
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

		// InitWidget creates the widget if the component has not drawn yet, so the first hit
		// can still update it (Epic Games, Inc., 2026b).
		WidgetComponent->InitWidget();
		return WidgetComponent->GetUserWidgetObject();
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
			if (UTextBlock* const DamageText = Cast<UTextBlock>(HealthWidget->GetWidgetFromName(TEXT("DamageText"))))
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
