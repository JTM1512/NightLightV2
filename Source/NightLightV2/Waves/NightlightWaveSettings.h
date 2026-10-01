#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "NightlightWaveSettings.generated.h"

class ANightlightEnemy;

// What an enemy type does in a wave, so the play-style counters can boost one role without knowing the class.
UENUM(BlueprintType)
enum class ENightlightWaveEnemyRole : uint8
{
	// Medium-speed melee enemy.
	Walker,

	// Fragile ranged enemy.
	Shade,

	// Slow, heavy area attacker.
	Brute
};

// One enemy type the wave director can buy with its threat budget (Epic Games, Inc., 2026b).
USTRUCT(BlueprintType)
struct NIGHTLIGHTV2_API FNightlightWaveEnemyEntry
{
	GENERATED_BODY()

	// Left empty in C++ so the Blueprint child with its mesh and health bar is chosen in the editor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves")
	TSubclassOf<ANightlightEnemy> EnemyClass;

	// Budget points one enemy of this type costs. At least 1, so spending always finishes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves", meta = (ClampMin = "1"))
	int32 ThreatCost = 1;

	// The first wave this type can appear in. Waves start at 1.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves", meta = (ClampMin = "1"))
	int32 UnlockWave = 1;

	// How likely this type is to be picked compared with the other unlocked types.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves", meta = (ClampMin = "0.0"))
	float BaseWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves")
	ENightlightWaveEnemyRole Role = ENightlightWaveEnemyRole::Walker;
};

// Every wave tuning number in one asset, so the balance pass never needs a C++ change. The defaults match
// the planning document, so the game still works before DA_WaveSettings is made (Epic Games, Inc., 2026a).
UCLASS(BlueprintType)
class NIGHTLIGHTV2_API UNightlightWaveSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UNightlightWaveSettings();

	// Enemy types the budget is spent on. Walker, Shade and Brute by default.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Enemies")
	TArray<FNightlightWaveEnemyEntry> Enemies;

	// B_0 in B_n = (B_0 + g * n) * A.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Budget", meta = (ClampMin = "0.0"))
	float StartingBudget = 6.0f;

	// g in B_n = (B_0 + g * n) * A. Linear growth keeps each wave a small step harder (Chen, 2007).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Budget", meta = (ClampMin = "0.0"))
	float BudgetGrowthPerWave = 3.0f;

	// A in the budget formula, nudged after each wave by the skill score
	// (Hunicke and Chapman, 2004).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "0.0"))
	float StartingAdaptiveFactor = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "0.0"))
	float MinAdaptiveFactor = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "0.0"))
	float MaxAdaptiveFactor = 1.35f;

	// Kept small so the player does not notice the difficulty changing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "0.0"))
	float AdaptiveStep = 0.1f;

	// A score above this raises A by AdaptiveStep.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RaiseScoreThreshold = 0.7f;

	// A score below this lowers A by AdaptiveStep.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowerScoreThreshold = 0.4f;

	// How much each measure counts towards the skill score. The planning document names
	// the four measures but not their weights, so they start equal.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "0.0"))
	float CoreHealthWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "0.0"))
	float EnemiesStoppedWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "0.0"))
	float ClearTimeWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "0.0"))
	float UnspentTokensWeight = 0.25f;

	// A wave cleared within its spawning time plus this scores 1 on time. The score
	// falls to 0 at twice that.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "0.0"))
	float ClearTimeGraceSeconds = 10.0f;

	// This many unspent tokens at the end of a wave gives a full token score.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive", meta = (ClampMin = "1"))
	int32 TokensForFullScore = 100;

	// Our reading of the document is that spare tokens mean the player is comfortable.
	// Untick to score spare tokens as the player struggling instead.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Adaptive")
	bool bSpareTokensRaiseScore = true;

	// Seconds between spawns in wave 1.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Spawn Gap", meta = (ClampMin = "0.1"))
	float FirstWaveSpawnGap = 2.0f;

	// Seconds between spawns from wave SpawnGapRampWaves onwards.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Spawn Gap", meta = (ClampMin = "0.1"))
	float FinalSpawnGap = 0.8f;

	// The wave where the gap reaches FinalSpawnGap and stops shrinking.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Spawn Gap", meta = (ClampMin = "1"))
	int32 SpawnGapRampWaves = 10;

	// Every Nth wave is a Brute wave: a peak, then a calmer wave after it (Booth, 2009).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Brute Waves", meta = (ClampMin = "1"))
	int32 BruteWaveEvery = 5;

	// Brute waves get a 25% larger budget by default.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Brute Waves", meta = (ClampMin = "1.0"))
	float BruteWaveBudgetMultiplier = 1.25f;

	// How strongly a Brute wave favours Brutes. The planning document gives no number, so this is our default.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Brute Waves", meta = (ClampMin = "1.0"))
	float BruteWaveBruteWeightMultiplier = 3.0f;

	// A longer first build phase so the player can read the map and place defenders.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Build Phase", meta = (ClampMin = "0.0"))
	float FirstBuildPhaseSeconds = 20.0f;

	// The countdown between later waves.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Build Phase", meta = (ClampMin = "0.0"))
	float BuildPhaseSeconds = 12.0f;

	// No build phase is shorter than this, so the player always has time to place a defender.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Build Phase", meta = (ClampMin = "0.0"))
	float MinBuildPhaseSeconds = 8.0f;

	// The share of a wave spawned slowly at the start, taken from the cheapest enemies.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Phases", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BuildUpShare = 0.4f;

	// The share spawned at the fastest gap, taken from the most expensive enemies.
	// Relief gets whatever is left after build-up and peak.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Phases", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PeakShare = 0.45f;

	// Build-up and relief spawn at the wave's spawn gap times this, so they feel slower.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Phases", meta = (ClampMin = "1.0"))
	float OffPeakGapMultiplier = 1.75f;

	// How far back the live challenge level looks when it counts Core damage and defenders lost.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Challenge", meta = (ClampMin = "1.0"))
	float ChallengeWindowSeconds = 10.0f;

	// Seconds between challenge level checks while a wave runs.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Challenge", meta = (ClampMin = "0.1"))
	float ChallengeCheckInterval = 1.0f;

	// Above this the player is struggling, so spawns are held back or the wave moves to relief early.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Challenge", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float HighChallengeThreshold = 70.0f;

	// Below this during build-up the player is coasting, so the peak starts early.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Challenge", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float LowChallengeThreshold = 30.0f;

	// How much each input counts towards the challenge level. They are divided by their total, so equal
	// values give equal weight.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Challenge", meta = (ClampMin = "0.0"))
	float CoreDamageChallengeWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Challenge", meta = (ClampMin = "0.0"))
	float DefendersLostChallengeWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Challenge", meta = (ClampMin = "0.0"))
	float EnemiesNearCoreChallengeWeight = 1.0f;

	// The share of the Core's max health lost inside the window that counts as full pressure.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Challenge", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float CoreDamageForFullChallenge = 0.2f;

	// Defenders destroyed inside the window that count as full pressure.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Challenge", meta = (ClampMin = "1"))
	int32 DefendersLostForFullChallenge = 2;

	// Enemies inside the Core's attack range that count as full pressure.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Challenge", meta = (ClampMin = "1"))
	int32 EnemiesNearCoreForFullChallenge = 4;

	// How long spawns may be held while the challenge stays high before the wave moves to relief instead.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Challenge", meta = (ClampMin = "0.0"))
	float MaxHoldSeconds = 6.0f;

	// Damage per second of defenders in reach that halves a route's spawn weight, so weak routes are picked more.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Routes", meta = (ClampMin = "1.0"))
	float RouteDefenceScale = 20.0f;

	// Each route's weight is multiplied by a random factor within plus or minus this share, rolled once per wave.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Routes", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float RouteRandomFactor = 0.2f;

	// JTM1512's idea as a weight, not a rule: Brutes lean towards the shortest route and Shades the longest.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Routes", meta = (ClampMin = "1.0"))
	float BruteShortestRouteMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves|Routes", meta = (ClampMin = "1.0"))
	float ShadeLongestRouteMultiplier = 1.5f;
};
