#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NightlightCrayonHUD.generated.h"

class UInputComponent;
class UNightlightCrayonHUDWidget;

// Set as the HUD Class in BP_TDGameMode to switch the game to the crayon HUD. It shows the HUD widget
// and adds the Space, F, Escape, U and click keys on top of the player controller's own keys, without
// taking 1 to 3 or the click away from it.
UCLASS()
class NIGHTLIGHTV2_API ANightlightCrayonHUD : public AHUD
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Nightlight|Crayon HUD")
	UNightlightCrayonHUDWidget* GetCrayonHUD() const { return HUDWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// A Widget Blueprint child of the C++ widget can be set here to add to the layout.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Crayon HUD")
	TSubclassOf<UNightlightCrayonHUDWidget> HUDWidgetClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNightlightCrayonHUDWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> CrayonInput;

	int32 LeftClickBindingIndex = INDEX_NONE;

	void HandleStartWave();
	void HandleSpeed();
	void HandlePause();
	void HandleNotebook();
	void HandleClick();
	void HandlePickFirst();
	void HandlePickSecond();
	void HandlePickThird();
};
