#pragma once

#include "CoreMinimal.h"

class AActor;
class UUserWidget;

// Enemies and defenders share these helpers so every world health bar updates the same way.
namespace NightlightHealthWidgetUtils
{
	// Returns the widget shown by the actor's first widget component, or null when it has none.
	UUserWidget* GetWorldHealthWidget(AActor* Actor);

	// Updates the HealthBar, HealthText and DamageText widgets by name when the widget has them.
	void UpdateWorldHealthWidget(
		AActor* Actor,
		double CurrentHealth,
		double MaxHealth,
		double DamageTaken = 0.0);
}
