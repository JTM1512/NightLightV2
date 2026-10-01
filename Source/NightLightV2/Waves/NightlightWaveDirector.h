#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "TimerManager.h"
#include "NightlightWaveMath.h"
#include "NightlightWaveDirector.generated.h"

class ANightlightDefender;
class ANightlightEnemy;
class ANightlightEnemySpawner;
class ANightlightDreamCore;
class UNightlightWaveSettings;

// What the HUD shows when a wave starts. Fields are only ever added here, so Blueprint bindings never break.
USTRUCT(BlueprintType)
struct NIGHTLIGHTV2_API FNightlightWaveInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	int32 WaveNumber = 0;

	// Every enemy planned for this wave, across all three phases.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	int32 EnemyCount = 0;

	// Every fifth wave has a bigger budget and leans towards Brutes.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	bool bIsPeakWave = false;

	// Empty until the wave templates are added.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	FName TemplateName;
};

// How the last wave went. Fields are only ever added here, so Blueprint bindings never break.
USTRUCT(BlueprintType)
struct NIGHTLIGHTV2_API FNightlightWaveSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	int32 WaveNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	bool bWasPeakWave = false;

	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	int32 EnemiesSpawned = 0;

	// Enemies that died before they reached the Core.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	int32 EnemiesStopped = 0;

	// Seconds from the wave starting until its last enemy was gone.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	float ClearSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	int32 ThreatBudget = 0;

	// The four measures the score was built from. Negative ones were not available.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	FNightlightWaveMeasures Measures;

	// 0 (played badly) to 1 (played well).
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	float PlayerScore = 0.0f;

	// A for the wave that just ended.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	float AdaptiveFactorBefore = 1.0f;

	// A for the next wave, after the score nudged it.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	float AdaptiveFactorAfter = 1.0f;

	// Seconds the live challenge level held spawns back during the wave.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	float HeldSeconds = 0.0f;

	// The player was struggling for too long, so the wave moved to relief before its peak finished.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	bool bReliefCameEarly = false;

	// The player was coasting, so the peak started before build-up finished.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	bool bPeakCameEarly = false;

	// The highest live challenge level reached during the wave, from 0 to 100.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	float HighestChallengeLevel = 0.0f;

	// The damage per second of the defenders in reach of each route when the wave started, by route index.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	TArray<float> RouteDefence;

	// Each route's spawn weight for the wave, before the Brute and Shade route preferences, by route index.
	UPROPERTY(BlueprintReadOnly, Category = "Nightlight|Waves")
	TArray<float> RouteWeights;
};

// How the director changed the pacing after a challenge level check.
UENUM(BlueprintType)
enum class ENightlightChallengeReaction : uint8
{
	// The player is struggling, so the spawn timer is paused.
	HeldSpawns,

	// The challenge dropped back down, so spawning carries on.
	ResumedSpawns,

	// Spawns were held for too long, so the rest of the phase moved into relief.
	ReliefEarly,

	// The player is coasting, so the rest of build-up moved into the peak.
	PeakEarly
};

// Dynamic so the wave HUD Blueprint can bind to them (Epic Games, Inc., 2026b).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNightlightBuildPhaseStartedSignature,
	int32, NextWaveNumber,
	float, BuildSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNightlightWaveStartedSignature,
	const FNightlightWaveInfo&, Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNightlightWavePhaseChangedSignature,
	ENightlightWavePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNightlightEnemiesRemainingChangedSignature,
	int32, EnemiesRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNightlightWaveClearedSignature,
	const FNightlightWaveSummary&, Summary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNightlightWavesStoppedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNightlightChallengeReactionSignature,
	ENightlightChallengeReaction, Reaction);

// Runs the wave loop: a build phase with a countdown, then a planned wave spawned in build-up, peak and relief,
// then the next build phase once every enemy from that wave is gone (Booth, 2009).
//
// For the wave HUD: on the widget's Construct, call Get Actor Of Class with this class once and keep the result.
// Bind the On... events below and read the Get... functions for the current values. Only the build countdown
// needs reading every frame (Get Build Time Remaining); everything else arrives through an event.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightWaveDirector : public AActor
{
	GENERATED_BODY()

public:
	ANightlightWaveDirector();

	// Prepares the spawner's routes and starts the first build phase. Runs by itself when Start On Begin Play is on.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Waves")
	void StartWaves();

	// Ends the build phase early, for a HUD "Start wave" button and for testing.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Waves")
	void StartNextWaveNow();

	// The wave running now, or the last one during a build phase. 0 before the first wave.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	int32 GetCurrentWaveNumber() const { return CurrentWaveNumber; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	ENightlightWavePhase GetWavePhase() const { return WavePhase; }

	// Seconds left on the build countdown, or 0 outside a build phase.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	float GetBuildTimeRemaining() const;

	// Enemies of this wave still waiting to spawn plus those still in play.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	int32 GetEnemiesRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	FNightlightWaveInfo GetCurrentWaveInfo() const { return CurrentWaveInfo; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	FNightlightWaveSummary GetLastWaveSummary() const { return LastWaveSummary; }

	// A in the budget formula: above 1 makes waves bigger, below 1 makes them smaller.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	float GetAdaptiveFactor() const { return AdaptiveFactor; }

	// The live challenge level from the last check, from 0 (coasting) to 100 (struggling). 0 outside a wave.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Waves")
	float GetChallengeLevel() const { return ChallengeLevel; }

	// The player's tokens at the end of a wave, for the skill score. C++ returns -1 (unknown), so the measure
	// is left out; BP_NightlightWaveDirector overrides this to read the token pool in the game state.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Nightlight|Waves")
	int32 GetUnspentTokens() const;

	// Passes the number of the coming wave and the length of the countdown.
	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Waves")
	FNightlightBuildPhaseStartedSignature OnBuildPhaseStarted;

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Waves")
	FNightlightWaveStartedSignature OnWaveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Waves")
	FNightlightWavePhaseChangedSignature OnWavePhaseChanged;

	// Fires whenever an enemy of this wave spawns or leaves play.
	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Waves")
	FNightlightEnemiesRemainingChangedSignature OnEnemiesRemainingChanged;

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Waves")
	FNightlightWaveClearedSignature OnWaveCleared;

	// Fires once when the Core is destroyed. No more waves run after it.
	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Waves")
	FNightlightWavesStoppedSignature OnWavesStopped;

	// Fires when the challenge level holds or resumes spawns, or brings relief or the peak forward.
	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Waves")
	FNightlightChallengeReactionSignature OnChallengeReaction;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Assign DA_WaveSettings. When empty, the C++ defaults from the planning document are used.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Waves")
	TObjectPtr<UNightlightWaveSettings> WaveSettings;

	// The spawner placed in the level. Untick its Spawn On Begin Play so only the director spawns.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Nightlight|Waves")
	TObjectPtr<ANightlightEnemySpawner> EnemySpawner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Waves")
	bool bStartOnBeginPlay = true;

private:
	// Settings->Enemies indices still to spawn in each phase. Kept as members so the challenge level can move
	// enemies between phases while the wave runs.
	TArray<int32> BuildUpEnemies;
	TArray<int32> PeakEnemies;
	TArray<int32> ReliefEnemies;

	// This wave's enemies still in play. Weak, so a destroyed enemy never stays alive through this set.
	TSet<TWeakObjectPtr<ANightlightEnemy>> WaveEnemies;

	UPROPERTY(Transient)
	TObjectPtr<ANightlightDreamCore> BoundDreamCore;

	FRandomStream WaveRandomStream;
	FTimerHandle BuildPhaseTimerHandle;
	FTimerHandle SpawnTimerHandle;
	FTimerHandle ChallengeTimerHandle;
	ENightlightWavePhase WavePhase = ENightlightWavePhase::NotStarted;
	int32 CurrentWaveNumber = 0;
	int32 CurrentWaveBudget = 0;
	int32 EnemiesSpawnedThisWave = 0;
	int32 EnemiesStoppedThisWave = 0;
	float WaveStartTime = 0.0f;
	float CoreHealthAtWaveStart = 0.0f;

	// How long the planned phases take to spawn, the base of the clear time target.
	float PlannedSpawnSeconds = 0.0f;

	// Seconds the in-wave challenge level held spawns back. Added to the clear time target, so a held wave is
	// not marked down for clearing slowly.
	float HeldSpawnSeconds = 0.0f;

	// A in the budget formula, nudged after each wave by the player's score.
	float AdaptiveFactor = 1.0f;

	// When the Core lost health in this wave and how much, as a share of its max health. Entries older than
	// the challenge window are dropped on each check.
	TArray<float> CoreDamageTimes;
	TArray<float> CoreDamageFractions;

	// When each defender was destroyed in this wave.
	TArray<float> DefenderLostTimes;

	// The Core's health at the last change, so each change can be stored as a drop.
	float LastKnownCoreHealth = 0.0f;

	float ChallengeLevel = 0.0f;
	float HighestChallengeLevel = 0.0f;
	bool bSpawnsHeld = false;
	float HoldStartTime = 0.0f;
	bool bReliefCameEarly = false;
	bool bPeakCameEarly = false;

	// Each route's length, worked out once after the routes are prepared, because routes never change.
	TArray<float> RouteLengths;

	// Each route's defence and spawn weight for this wave. Worked out once when the wave starts, so each route's
	// random factor is rolled once per wave and the weights stay steady while the wave spawns.
	TArray<float> RouteDefence;
	TArray<float> RouteBaseWeights;

	FNightlightWaveInfo CurrentWaveInfo;
	FNightlightWaveSummary LastWaveSummary;

	const UNightlightWaveSettings* GetSettings() const;
	void StartBuildPhase(float Seconds);
	void StartWave();
	void StartPhase(ENightlightWavePhase Phase);
	void SpawnNextPhaseEnemy();
	void UpdateRouteWeights();
	void TryFinishWave();
	FNightlightWaveMeasures BuildWaveMeasures(float ClearSeconds) const;
	void StopWaves();
	void SetWavePhase(ENightlightWavePhase NewPhase);
	void BroadcastEnemiesRemaining();
	bool HasEnemiesToSpawn() const;
	TArray<int32>* GetPhaseEnemies(ENightlightWavePhase Phase);
	bool IsWaveRunning() const;
	void UpdateChallengeLevel();
	void HoldSpawns();
	void EndHold();
	void ReactToChallengeLevel();

	UFUNCTION()
	void HandleEnemyRemoved(ANightlightEnemy* Enemy);

	UFUNCTION()
	void HandleCoreDestroyed();

	UFUNCTION()
	void HandleCoreHealthChanged(float CurrentHealth, float MaxHealth);

	UFUNCTION()
	void HandleDefenderRemoved(ANightlightDefender* Defender);
};
