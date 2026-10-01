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

// Where the wave director is in its loop. The HUD can show a different message for each one.
UENUM(BlueprintType)
enum class ENightlightWavePhase : uint8
{
	// Waiting for the generator to finish its routes.
	NotStarted,

	// Between waves, while the player builds. The countdown is running.
	BuildPhase,

	// The start of a wave: mostly cheap enemies, spawned slowly.
	BuildUp,

	// The middle of a wave: the most expensive enemies, spawned fastest.
	Peak,

	// The end of a wave: the rest, spawned slowly again.
	Relief,

	// The Core was destroyed, so no more waves run.
	Stopped
};

// The planned enemies split into the three phases of a wave, as Settings->Enemies indices in spawn order.
USTRUCT(BlueprintType)
struct NIGHTLIGHTV2_API FNightlightWavePhaseLists
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Waves")
	TArray<int32> BuildUp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Waves")
	TArray<int32> Peak;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Waves")
	TArray<int32> Relief;
};

// The wave maths on its own, so the wave director stays small and the tests can call it directly
// (Epic Games, Inc., 2026d). A null Settings uses the C++ defaults from the planning document.
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

	// Splits a planned wave into build-up, peak and relief by the shares in Settings. The most expensive
	// enemies go to the peak, the cheapest of the rest to build-up and the remainder to relief. Each list is
	// then shuffled with the stream, so the same seed always gives the same split.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Waves")
	static FNightlightWavePhaseLists SplitIntoPhases(
		const UNightlightWaveSettings* Settings,
		const TArray<int32>& PlannedEntryIndices,
		UPARAM(ref) FRandomStream& RandomStream);

	// The wave's spawn gap for the peak, and that gap times OffPeakGapMultiplier for every other phase.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	static float GetPhaseSpawnGap(const UNightlightWaveSettings* Settings, int32 WaveNumber, ENightlightWavePhase Phase);
};
