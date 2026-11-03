#pragma once

#include "CoreMinimal.h"
#include "NightlightCrayonWidgets.h"
#include "NightlightCrayonScreens.generated.h"

class UVerticalBox;

// The crayon pause menu and game over screen. They replace WBP_PaueMenu and WBP_GameOver and keep the
// same three actions: resume, restart the level and quit.
UCLASS()
class NIGHTLIGHTV2_API UNightlightCrayonScreen : public UNightlightCrayonWidget
{
	GENERATED_BODY()

public:
	void SetupPause();

	// NightsSurvived is the wave number the Core fell on.
	void SetupGameOver(int32 NightsSurvived);

	// Unpauses and removes the screen. The player controller's Is Paused flag is cleared too, so its
	// own pause key keeps working.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Crayon HUD")
	void Resume();

protected:
	virtual void BuildTree() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UImage> Backdrop;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Column;

	UNightlightCrayonButton* AddButton(ENightlightCrayonButtonKind Kind, const FText& Label);

	UFUNCTION()
	void HandleRestart();

	UFUNCTION()
	void HandleQuit();
};
