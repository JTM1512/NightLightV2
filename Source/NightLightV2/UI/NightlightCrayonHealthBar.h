#pragma once

#include "CoreMinimal.h"
#include "NightlightCrayonWidgets.h"
#include "NightlightCrayonHealthBar.generated.h"

class UProgressBar;

// The crayon version of WBP_WorldHealthBar: a small strip of paper with crayon hatching for health over
// a smear of ink. It keeps the HealthBar, HealthText and DamageText names, so NightlightHealthWidgetUtils
// updates it exactly like the old bar.
UCLASS()
class NIGHTLIGHTV2_API UNightlightCrayonHealthBar : public UNightlightCrayonWidget
{
	GENERATED_BODY()

public:
	// Defenders are the kid's drawings, so they get green hatching. Nightmares get red.
	void SetIsNightmare(bool bInIsNightmare);

	// Shows "-N" in dripping red that floats up and fades.
	void ShowDamage(float DamageAmount);

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DamageText;

	float DamageAge = -1.0f;
};
