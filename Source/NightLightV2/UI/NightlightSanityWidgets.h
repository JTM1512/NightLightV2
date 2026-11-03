#pragma once

#include "CoreMinimal.h"
#include "NightlightCrayonWidgets.h"
#include "NightlightSanityLibrary.h"
#include "Styling/SlateTypes.h"
#include "NightlightSanityWidgets.generated.h"

class UMaterialInstanceDynamic;
class UOverlaySlot;
class UProgressBar;

// The tug of war across the top: the kid's crayon sun on the left pushes against the ink monster on the
// right. The yellow hatching is his sanity, which is the Dream Core's health.
UCLASS()
class NIGHTLIGHTV2_API UNightlightSanityBar : public UNightlightCrayonWidget
{
	GENERATED_BODY()

public:
	// Percent is from 0 to 1. Called every frame with the smoothed value, so it only does cheap work.
	void SetSanity(float Percent, ENightlightSanityState State);

protected:
	virtual void BuildTree() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RedEdge;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightBoilImage> KidSun;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightBoilImage> Beast;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	TOptional<ENightlightSanityState> ShownState;
	FProgressBarStyle BarStyle;
	FSlateBrush YellowFill;
	FSlateBrush GreyFill;
};

// The nightmare ink that creeps in from the screen edges as sanity drops, with red eyes opening in it
// below 30%. It never takes clicks, so it can cover the map without blocking play.
UCLASS()
class NIGHTLIGHTV2_API UNightlightInkOverlay : public UNightlightCrayonWidget
{
	GENERATED_BODY()

public:
	void UpdateInk(float Percent, ENightlightSanityState State, float DeltaTime);

protected:
	virtual void BuildTree() override;

private:
	// The smooth version: one material masks the ink with noise against Sanity.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> InkMaterial;

	// The simple version, used when the material has not been made yet: four vignettes cross-faded.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> Vignettes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> Eyes;

	float EyeOpacity = 0.0f;
	float EyeClock = 0.0f;
};
