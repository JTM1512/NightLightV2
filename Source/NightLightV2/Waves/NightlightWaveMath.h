#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/RandomStream.h"
#include "NightlightWaveSettings.h"
#include "NightlightWaveMath.generated.h"

// Multipliers on each role's base weight. Sprint 5 fills these in from the player's defender layout.
USTRUCT(BlueprintType)
struct NIGHTLIGHTV2_API FNightlightEnemyMixWeights
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Waves", meta = (ClampMin = "0.0"))
	float Walker = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Waves", meta = (ClampMin = "0.0"))
	float Shade = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Waves", meta = (ClampMin = "0.0"))
	float Brute = 1.0f;

	float GetWeightForRole(ENightlightWaveEnemyRole Role) const;
};

// The wave maths on its own, so the wave director stays small and the tests can call it directly
// (Epic Games, Inc., 2026c). A null Settings uses the C++ defaults from the planning document.
UCLASS()
class NIGHTLIGHTV2_API UNightlightWaveMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// True on every BruteWaveEvery-th wave (5, 10, 15...).
	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	static bool IsBruteWave(const UNightlightWaveSettings* Settings, int32 WaveNumber);

	// (B_0 + g * n) * A, times the Brute wave multiplier on Brute waves, rounded to whole threat points
	// and never below 1. Waves start at 1.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	static int32 CalculateWaveBudget(const UNightlightWaveSettings* Settings, int32 WaveNumber, float AdaptiveFactor);

	// Seconds between spawns: shrinks in a straight line until SpawnGapRampWaves, then stays flat.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	static float CalculateSpawnGap(const UNightlightWaveSettings* Settings, int32 WaveNumber);

	// Spends the budget on unlocked, affordable enemy types by weighted random picks and returns the
	// Settings->Enemies index of each enemy in spawn order. The same stream seed always gives the same list.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Waves")
	static TArray<int32> PlanWaveEnemies(
		const UNightlightWaveSettings* Settings,
		int32 WaveNumber,
		int32 Budget,
		const FNightlightEnemyMixWeights& TypeWeightMultipliers,
		UPARAM(ref) FRandomStream& RandomStream);
};
