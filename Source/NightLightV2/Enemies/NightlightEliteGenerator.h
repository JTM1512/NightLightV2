#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NightlightEliteTypes.h"
#include "../Waves/NightlightWaveMath.h"
#include "NightlightEliteGenerator.generated.h"

class UNightlightWaveSettings;

// The elite maths on its own, like the wave maths, so the director stays small and the tests can call it
// directly (Epic Games, Inc., 2026c). A null Settings uses the C++ defaults.
UCLASS()
class NIGHTLIGHTV2_API UNightlightEliteGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// The chance each planned enemy is promoted: 0 before EliteStartWave, then
	// min(EliteBaseChance + EliteChancePerWave * (n - EliteStartWave), EliteMaxChance).
	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy|Elite")
	static float CalculateEliteChance(const UNightlightWaveSettings* Settings, int32 WaveNumber);

	// One mutation before TwoMutationWave and two from it.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy|Elite")
	static int32 GetMutationCount(const UNightlightWaveSettings* Settings, int32 WaveNumber);

	// One seed per elite from the map seed, the wave and the enemy's place in the wave's plan, so the same map
	// always gives the same elites.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy|Elite")
	static int32 MakeEliteSeed(int32 MapSeed, int32 WaveNumber, int32 PlanIndex);

	// A mutation's pick weight: its rule's Weight, times EliteCounterWeightMultiplier when it counters the play style.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy|Elite")
	static float GetMutationWeight(const UNightlightWaveSettings* Settings, const FNightlightEliteMutationRule& Rule,
		const FNightlightPlayStyle& PlayStyle);

	// Picks the mutations without repeats or excluded pairs, rolls each one's strength, then combines and clamps
	// the stats and works out the look and name. The same arguments always give the same profile.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy|Elite")
	static FNightlightEliteProfile GenerateEliteProfile(const UNightlightWaveSettings* Settings, int32 WaveNumber,
		int32 Seed, const FNightlightPlayStyle& PlayStyle, ENightlightWaveEnemyRole BaseRole);

	// The mutations and the base type, such as "Swift Siphon Shade".
	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy|Elite")
	static FText BuildEliteDisplayName(const TArray<ENightlightEliteMutation>& Mutations, ENightlightWaveEnemyRole BaseRole);

	// The base tokens times the profile's token multiplier, rounded to whole tokens.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy|Elite")
	static int32 CalculateEliteTokens(int32 BaseTokens, const FNightlightEliteProfile& Profile);
};
