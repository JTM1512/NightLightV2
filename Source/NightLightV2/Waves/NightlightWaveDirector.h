#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "TimerManager.h"
#include "NightlightWaveMath.h"
#include "NightlightWaveDirector.generated.h"

class ANightlightEnemy;
class ANightlightEnemySpawner;
class ANightlightDreamCore;
class UNightlightWaveSettings;

// What the HUD shows when a wave starts. Later sprints only add fields here, so Blueprint bindings never break.
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

// How the last wave went. Later sprints only add fields here, so Blueprint bindings never break.
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
};

// Dynamic so the wave HUD Blueprint can bind to them (Epic Games, Inc., 2026a).
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
	// Settings->Enemies indices still to spawn in each phase. Kept as members so a later sprint can move
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
	ENightlightWavePhase WavePhase = ENightlightWavePhase::NotStarted;
	int32 CurrentWaveNumber = 0;
	int32 CurrentWaveBudget = 0;
	int32 NextRouteIndex = 0;
	int32 EnemiesSpawnedThisWave = 0;
	int32 EnemiesStoppedThisWave = 0;
	float WaveStartTime = 0.0f;

	// A in the budget formula. It stays at its starting value until the skill score is added.
	float AdaptiveFactor = 1.0f;

	FNightlightWaveInfo CurrentWaveInfo;
	FNightlightWaveSummary LastWaveSummary;

	const UNightlightWaveSettings* GetSettings() const;
	void StartBuildPhase(float Seconds);
	void StartWave();
	void StartPhase(ENightlightWavePhase Phase);
	void SpawnNextPhaseEnemy();
	void TryFinishWave();
	void StopWaves();
	void SetWavePhase(ENightlightWavePhase NewPhase);
	void BroadcastEnemiesRemaining();
	bool HasEnemiesToSpawn() const;
	TArray<int32>* GetPhaseEnemies(ENightlightWavePhase Phase);

	UFUNCTION()
	void HandleEnemyRemoved(ANightlightEnemy* Enemy);

	UFUNCTION()
	void HandleCoreDestroyed();
};
