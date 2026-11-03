#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NightlightSanityLibrary.generated.h"

// How the kid is holding up. The crayon HUD changes its whole mood when this changes.
UENUM(BlueprintType)
enum class ENightlightSanityState : uint8
{
	Brave,
	Scared,
	LosingIt
};

// One shared rule for when the HUD changes mood, so every widget agrees (Epic Games, Inc., 2026).
UCLASS()
class NIGHTLIGHTV2_API UNightlightSanityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Percent is the Core's health percent, from 0 to 1. Below ScaredBelow the kid is Scared, and below
	// LosingItBelow he is Losing It.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Sanity")
	static ENightlightSanityState GetSanityState(
		float Percent,
		float ScaredBelow = 0.6f,
		float LosingItBelow = 0.3f);

	// Which of the four ink vignettes fits this sanity: 0 Brave, 1 Scared, 2 Losing, 3 Overrun.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Sanity")
	static int32 GetInkStage(float Percent, float OverrunBelow = 0.15f);

	// Swaps a few letters for numbers, so "Shooter" reads "Sh00ter" when the nightmare takes over.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Sanity")
	static FText CorruptText(const FText& Text);
};

/*
References

Epic Games, Inc., 2026. Blueprint Function Libraries in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/blueprint-function-libraries-in-unreal-engine>
[Accessed 3 November 2026].
*/
