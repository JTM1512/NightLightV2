#include "NightlightWaveDirector.h"
#include "NightlightWaveSettings.h"
#include "../Core/NightlightDreamCore.h"
#include "../Defenders/NightlightDefender.h"
#include "../Enemies/NightlightEnemy.h"
#include "../Enemies/NightlightEnemySpawner.h"
#include "../ProceduralGeneration/NightlightWorldGenerator.h"
#include "../Systems/NightlightActorRegistrySubsystem.h"
#include "Engine/World.h"

ANightlightWaveDirector::ANightlightWaveDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ANightlightWaveDirector::BeginPlay()
{
	Super::BeginPlay();

	// The registry tells the director when each enemy leaves play, so it never has to search for them
	// (Epic Games, Inc., 2026f).
	if (UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>())
	{
		Registry->OnEnemyRemoved.AddDynamic(this, &ANightlightWaveDirector::HandleEnemyRemoved);
		Registry->OnDefenderRemoved.AddDynamic(this, &ANightlightWaveDirector::HandleDefenderRemoved);
	}

	if (bStartOnBeginPlay)
	{
		// Wait one tick, because the generator builds its routes in its own BeginPlay (Epic Games, Inc., 2026d).
		GetWorldTimerManager().SetTimerForNextTick(this, &ANightlightWaveDirector::StartWaves);
	}
}

void ANightlightWaveDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(BuildPhaseTimerHandle);
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(ChallengeTimerHandle);

	if (UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>())
	{
		Registry->OnEnemyRemoved.RemoveDynamic(this, &ANightlightWaveDirector::HandleEnemyRemoved);
		Registry->OnDefenderRemoved.RemoveDynamic(this, &ANightlightWaveDirector::HandleDefenderRemoved);
	}

	if (IsValid(BoundDreamCore))
	{
		BoundDreamCore->OnCoreDestroyed.RemoveDynamic(this, &ANightlightWaveDirector::HandleCoreDestroyed);
		BoundDreamCore->OnCoreHealthChanged.RemoveDynamic(this, &ANightlightWaveDirector::HandleCoreHealthChanged);
	}

	Super::EndPlay(EndPlayReason);
}

const UNightlightWaveSettings* ANightlightWaveDirector::GetSettings() const
{
	// The class defaults match the planning document, so the director still runs before DA_WaveSettings is set.
	return WaveSettings ? WaveSettings.Get() : GetDefault<UNightlightWaveSettings>();
}

void ANightlightWaveDirector::StartWaves()
{
	if (WavePhase != ENightlightWavePhase::NotStarted)
	{
		return;
	}

	if (!EnemySpawner)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight waves did not start because no Enemy Spawner is assigned."));
		return;
	}

	if (!EnemySpawner->PrepareRoutes())
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight waves did not start because the Enemy Spawner could not prepare its routes."));
		return;
	}

	// The map's seed also seeds the waves, so the same map always plays the same waves while balancing
	// (Epic Games, Inc., 2026c).
	const ANightlightWorldGenerator* const Generator = EnemySpawner->GetWorldGenerator();
	WaveRandomStream.Initialize(Generator ? Generator->GetActiveSeed() : 0);

	BoundDreamCore = EnemySpawner->GetDreamCore();
	if (IsValid(BoundDreamCore))
	{
		BoundDreamCore->OnCoreDestroyed.AddDynamic(this, &ANightlightWaveDirector::HandleCoreDestroyed);
		BoundDreamCore->OnCoreHealthChanged.AddDynamic(this, &ANightlightWaveDirector::HandleCoreHealthChanged);
		LastKnownCoreHealth = BoundDreamCore->GetCurrentHealth();
		if (BoundDreamCore->IsCoreDestroyed())
		{
			StopWaves();
			return;
		}
	}

	const UNightlightWaveSettings* const Settings = GetSettings();
	AdaptiveFactor = FMath::Clamp(Settings->StartingAdaptiveFactor, Settings->MinAdaptiveFactor, Settings->MaxAdaptiveFactor);
	CurrentWaveNumber = 0;
	NextRouteIndex = 0;
	StartBuildPhase(Settings->FirstBuildPhaseSeconds);
}

void ANightlightWaveDirector::StartNextWaveNow()
{
	if (WavePhase == ENightlightWavePhase::BuildPhase)
	{
		StartWave();
	}
}

float ANightlightWaveDirector::GetBuildTimeRemaining() const
{
	if (WavePhase != ENightlightWavePhase::BuildPhase)
	{
		return 0.0f;
	}

	// The timer manager returns -1 for a timer that is not running (Epic Games, Inc., 2026d).
	return FMath::Max(GetWorldTimerManager().GetTimerRemaining(BuildPhaseTimerHandle), 0.0f);
}

int32 ANightlightWaveDirector::GetEnemiesRemaining() const
{
	return BuildUpEnemies.Num() + PeakEnemies.Num() + ReliefEnemies.Num() + WaveEnemies.Num();
}

void ANightlightWaveDirector::StartBuildPhase(const float Seconds)
{
	const float BuildSeconds = FMath::Max(Seconds, GetSettings()->MinBuildPhaseSeconds);

	// A one-shot timer starts the next wave when the countdown ends (Epic Games, Inc., 2026e).
	GetWorldTimerManager().SetTimer(
		BuildPhaseTimerHandle,
		this,
		&ANightlightWaveDirector::StartWave,
		BuildSeconds,
		false);

	SetWavePhase(ENightlightWavePhase::BuildPhase);
	OnBuildPhaseStarted.Broadcast(CurrentWaveNumber + 1, BuildSeconds);
}

void ANightlightWaveDirector::StartWave()
{
	GetWorldTimerManager().ClearTimer(BuildPhaseTimerHandle);
	++CurrentWaveNumber;

	const UNightlightWaveSettings* const Settings = GetSettings();
	CurrentWaveBudget = UNightlightWaveMath::CalculateWaveBudget(Settings, CurrentWaveNumber, AdaptiveFactor);

	// Neutral weights until the play-style counters are added.
	const FNightlightEnemyMixWeights MixWeights;
	const TArray<int32> PlannedEntries = UNightlightWaveMath::PlanWaveEnemies(
		Settings, CurrentWaveNumber, CurrentWaveBudget, MixWeights, WaveRandomStream);

	const FNightlightWavePhaseLists PhaseLists = UNightlightWaveMath::SplitIntoPhases(Settings, PlannedEntries, WaveRandomStream);
	BuildUpEnemies = PhaseLists.BuildUp;
	PeakEnemies = PhaseLists.Peak;
	ReliefEnemies = PhaseLists.Relief;

	WaveEnemies.Reset();
	EnemiesSpawnedThisWave = 0;
	EnemiesStoppedThisWave = 0;
	WaveStartTime = GetWorld()->GetTimeSeconds();
	CoreHealthAtWaveStart = IsValid(BoundDreamCore) ? BoundDreamCore->GetCurrentHealth() : 0.0f;
	PlannedSpawnSeconds = UNightlightWaveMath::CalculatePlannedSpawnSeconds(Settings, CurrentWaveNumber, PhaseLists);
	HeldSpawnSeconds = 0.0f;

	// The challenge level only looks at this wave, so nothing from the last wave carries over.
	CoreDamageTimes.Reset();
	CoreDamageFractions.Reset();
	DefenderLostTimes.Reset();
	LastKnownCoreHealth = CoreHealthAtWaveStart;
	ChallengeLevel = 0.0f;
	HighestChallengeLevel = 0.0f;
	bSpawnsHeld = false;
	bReliefCameEarly = false;
	bPeakCameEarly = false;

	CurrentWaveInfo = FNightlightWaveInfo();
	CurrentWaveInfo.WaveNumber = CurrentWaveNumber;
	CurrentWaveInfo.EnemyCount = PlannedEntries.Num();
	CurrentWaveInfo.bIsPeakWave = UNightlightWaveMath::IsBruteWave(Settings, CurrentWaveNumber);

	if (PlannedEntries.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight wave %d planned no enemies. Check that every row in Wave Settings has an Enemy Class."), CurrentWaveNumber);
	}

	OnWaveStarted.Broadcast(CurrentWaveInfo);
	BroadcastEnemiesRemaining();

	// Started before the first phase, because an empty wave finishes inside StartPhase and clears this timer
	// (Epic Games, Inc., 2026e).
	GetWorldTimerManager().SetTimer(
		ChallengeTimerHandle,
		this,
		&ANightlightWaveDirector::UpdateChallengeLevel,
		FMath::Max(Settings->ChallengeCheckInterval, 0.1f),
		true);

	StartPhase(ENightlightWavePhase::BuildUp);
}

void ANightlightWaveDirector::StartPhase(ENightlightWavePhase Phase)
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);

	// Skip any phase with nothing to spawn. Relief is the last one.
	while (GetPhaseEnemies(Phase) && GetPhaseEnemies(Phase)->IsEmpty() && Phase != ENightlightWavePhase::Relief)
	{
		Phase = Phase == ENightlightWavePhase::BuildUp ? ENightlightWavePhase::Peak : ENightlightWavePhase::Relief;
	}

	const TArray<int32>* const PhaseEnemies = GetPhaseEnemies(Phase);
	SetWavePhase(Phase);
	if (!PhaseEnemies || PhaseEnemies->IsEmpty())
	{
		// Nothing was planned, so the wave ends at once and the next build phase starts.
		TryFinishWave();
		return;
	}

	// The first enemy of a phase spawns at once, then a repeating timer spawns the rest at this phase's gap.
	SpawnNextPhaseEnemy();
	if (HasEnemiesToSpawn())
	{
		GetWorldTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&ANightlightWaveDirector::SpawnNextPhaseEnemy,
			UNightlightWaveMath::GetPhaseSpawnGap(GetSettings(), CurrentWaveNumber, Phase),
			true);
	}
}

void ANightlightWaveDirector::SpawnNextPhaseEnemy()
{
	TArray<int32>* const PhaseEnemies = GetPhaseEnemies(WavePhase);
	if (!PhaseEnemies)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		return;
	}

	if (PhaseEnemies->IsEmpty())
	{
		// This phase has spawned everything, so the next one starts one gap after its last enemy.
		StartPhase(WavePhase == ENightlightWavePhase::BuildUp ? ENightlightWavePhase::Peak : ENightlightWavePhase::Relief);
		return;
	}

	const int32 EntryIndex = (*PhaseEnemies)[0];
	PhaseEnemies->RemoveAt(0);

	const UNightlightWaveSettings* const Settings = GetSettings();
	const int32 RouteCount = EnemySpawner ? EnemySpawner->GetRouteCount() : 0;
	if (Settings->Enemies.IsValidIndex(EntryIndex) && RouteCount > 0)
	{
		// Routes are taken in turn until the weighted route picks are added.
		const int32 RouteIndex = NextRouteIndex % RouteCount;
		NextRouteIndex = (RouteIndex + 1) % RouteCount;

		if (ANightlightEnemy* const Enemy = EnemySpawner->SpawnEnemyOnRoute(Settings->Enemies[EntryIndex].EnemyClass, RouteIndex))
		{
			// A weak pointer, so an enemy destroyed without its EndPlay reaching us is still dropped later
			// (Epic Games, Inc., 2026g).
			WaveEnemies.Add(Enemy);
			++EnemiesSpawnedThisWave;
		}
	}

	BroadcastEnemiesRemaining();

	if (!HasEnemiesToSpawn())
	{
		// Every enemy is out, so the wave clears as soon as the last one leaves play.
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		TryFinishWave();
	}
}

void ANightlightWaveDirector::TryFinishWave()
{
	if (WavePhase == ENightlightWavePhase::NotStarted
		|| WavePhase == ENightlightWavePhase::BuildPhase
		|| WavePhase == ENightlightWavePhase::Stopped
		|| HasEnemiesToSpawn())
	{
		return;
	}

	// Drop any entry whose enemy was destroyed without telling the registry.
	for (auto It = WaveEnemies.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	if (!WaveEnemies.IsEmpty())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(ChallengeTimerHandle);
	EndHold();

	LastWaveSummary = FNightlightWaveSummary();
	LastWaveSummary.WaveNumber = CurrentWaveNumber;
	LastWaveSummary.bWasPeakWave = CurrentWaveInfo.bIsPeakWave;
	LastWaveSummary.EnemiesSpawned = EnemiesSpawnedThisWave;
	LastWaveSummary.EnemiesStopped = EnemiesStoppedThisWave;
	LastWaveSummary.ClearSeconds = GetWorld()->GetTimeSeconds() - WaveStartTime;
	LastWaveSummary.ThreatBudget = CurrentWaveBudget;
	LastWaveSummary.HeldSeconds = HeldSpawnSeconds;
	LastWaveSummary.bReliefCameEarly = bReliefCameEarly;
	LastWaveSummary.bPeakCameEarly = bPeakCameEarly;
	LastWaveSummary.HighestChallengeLevel = HighestChallengeLevel;
	ChallengeLevel = 0.0f;

	// Score how the player handled the wave and nudge A, which scales the next wave's budget
	// (Hunicke and Chapman, 2004).
	const UNightlightWaveSettings* const Settings = GetSettings();
	LastWaveSummary.Measures = BuildWaveMeasures(LastWaveSummary.ClearSeconds);
	LastWaveSummary.PlayerScore = UNightlightWaveMath::CalculatePlayerScore(Settings, LastWaveSummary.Measures);
	LastWaveSummary.AdaptiveFactorBefore = AdaptiveFactor;
	AdaptiveFactor = UNightlightWaveMath::UpdateAdaptiveFactor(Settings, AdaptiveFactor, LastWaveSummary.PlayerScore);
	LastWaveSummary.AdaptiveFactorAfter = AdaptiveFactor;
	OnWaveCleared.Broadcast(LastWaveSummary);

	// Build-up, peak, then a calm build phase before the next wave (Booth, 2009).
	if (WavePhase != ENightlightWavePhase::Stopped)
	{
		StartBuildPhase(GetSettings()->BuildPhaseSeconds);
	}
}

FNightlightWaveMeasures ANightlightWaveDirector::BuildWaveMeasures(const float ClearSeconds) const
{
	const UNightlightWaveSettings* const Settings = GetSettings();
	FNightlightWaveMeasures Measures;

	// Each measure stays at -1 (left out) when there is nothing to divide by.
	if (IsValid(BoundDreamCore) && CoreHealthAtWaveStart > 0.0f)
	{
		Measures.CoreHealthKept = FMath::Clamp(BoundDreamCore->GetCurrentHealth() / CoreHealthAtWaveStart, 0.0f, 1.0f);
	}

	if (EnemiesSpawnedThisWave > 0)
	{
		Measures.EnemiesStopped = static_cast<float>(EnemiesStoppedThisWave) / EnemiesSpawnedThisWave;
	}

	// Held seconds are added to the target, so a wave the challenge level slowed down is not marked down.
	Measures.ClearTime = UNightlightWaveMath::CalculateClearTimeScore(Settings, ClearSeconds, PlannedSpawnSeconds + HeldSpawnSeconds);
	Measures.UnspentTokens = UNightlightWaveMath::CalculateUnspentTokensScore(Settings, GetUnspentTokens());
	return Measures;
}

int32 ANightlightWaveDirector::GetUnspentTokens_Implementation() const
{
	// The token pool lives in the Blueprint game state, so C++ cannot read it. The Blueprint child overrides
	// this native event to return it (Epic Games, Inc., 2026h).
	return -1;
}

void ANightlightWaveDirector::StopWaves()
{
	GetWorldTimerManager().ClearTimer(BuildPhaseTimerHandle);
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(ChallengeTimerHandle);
	bSpawnsHeld = false;
	ChallengeLevel = 0.0f;
	BuildUpEnemies.Reset();
	PeakEnemies.Reset();
	ReliefEnemies.Reset();

	SetWavePhase(ENightlightWavePhase::Stopped);
	OnWavesStopped.Broadcast();
}

void ANightlightWaveDirector::SetWavePhase(const ENightlightWavePhase NewPhase)
{
	if (WavePhase == NewPhase)
	{
		return;
	}

	WavePhase = NewPhase;
	OnWavePhaseChanged.Broadcast(WavePhase);
}

void ANightlightWaveDirector::BroadcastEnemiesRemaining()
{
	OnEnemiesRemainingChanged.Broadcast(GetEnemiesRemaining());
}

bool ANightlightWaveDirector::HasEnemiesToSpawn() const
{
	return !BuildUpEnemies.IsEmpty() || !PeakEnemies.IsEmpty() || !ReliefEnemies.IsEmpty();
}

TArray<int32>* ANightlightWaveDirector::GetPhaseEnemies(const ENightlightWavePhase Phase)
{
	switch (Phase)
	{
	case ENightlightWavePhase::BuildUp:
		return &BuildUpEnemies;
	case ENightlightWavePhase::Peak:
		return &PeakEnemies;
	case ENightlightWavePhase::Relief:
		return &ReliefEnemies;
	default:
		return nullptr;
	}
}

bool ANightlightWaveDirector::IsWaveRunning() const
{
	return WavePhase == ENightlightWavePhase::BuildUp
		|| WavePhase == ENightlightWavePhase::Peak
		|| WavePhase == ENightlightWavePhase::Relief;
}

void ANightlightWaveDirector::UpdateChallengeLevel()
{
	if (!IsWaveRunning())
	{
		GetWorldTimerManager().ClearTimer(ChallengeTimerHandle);
		return;
	}

	const UNightlightWaveSettings* const Settings = GetSettings();
	const float Now = GetWorld()->GetTimeSeconds();
	const float WindowStart = Now - Settings->ChallengeWindowSeconds;

	// Events are added in time order, so the old ones are always at the front of each array
	// (Epic Games, Inc., 2026a).
	while (!CoreDamageTimes.IsEmpty() && CoreDamageTimes[0] < WindowStart)
	{
		CoreDamageTimes.RemoveAt(0);
		CoreDamageFractions.RemoveAt(0);
	}
	while (!DefenderLostTimes.IsEmpty() && DefenderLostTimes[0] < WindowStart)
	{
		DefenderLostTimes.RemoveAt(0);
	}

	float CoreDamageFraction = 0.0f;
	for (const float Fraction : CoreDamageFractions)
	{
		CoreDamageFraction += Fraction;
	}

	// Enemies near the Core are counted now rather than stored, because they can walk away or die at any time.
	int32 EnemiesNearCore = 0;
	const UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>();
	if (Registry && IsValid(BoundDreamCore))
	{
		const FVector CoreLocation = BoundDreamCore->GetActorLocation();
		const float RangeSquared = FMath::Square(BoundDreamCore->GetAttackRange());
		for (const ANightlightEnemy* const Enemy : Registry->GetEnemies())
		{
			if (IsValid(Enemy) && !Enemy->IsDead() && FVector::DistSquared(Enemy->GetActorLocation(), CoreLocation) <= RangeSquared)
			{
				++EnemiesNearCore;
			}
		}
	}

	ChallengeLevel = UNightlightWaveMath::CalculateChallengeLevel(Settings, CoreDamageFraction, DefenderLostTimes.Num(), EnemiesNearCore);
	HighestChallengeLevel = FMath::Max(HighestChallengeLevel, ChallengeLevel);
	ReactToChallengeLevel();
}

void ANightlightWaveDirector::ReactToChallengeLevel()
{
	// JTM1512's idea: the pacing follows the player during the wave, but the enemies in it never change, so
	// the player gets a breather without the wave becoming easier (Hunicke and Chapman, 2004). Relief is never
	// held or sped up, so the wave always finishes.
	if (WavePhase != ENightlightWavePhase::BuildUp && WavePhase != ENightlightWavePhase::Peak)
	{
		return;
	}

	const UNightlightWaveSettings* const Settings = GetSettings();
	const float Now = GetWorld()->GetTimeSeconds();

	if (bSpawnsHeld)
	{
		if (ChallengeLevel <= Settings->HighChallengeThreshold)
		{
			// Unpausing carries on with the time the spawn timer had left (Epic Games, Inc., 2026d).
			EndHold();
			GetWorldTimerManager().UnPauseTimer(SpawnTimerHandle);
			OnChallengeReaction.Broadcast(ENightlightChallengeReaction::ResumedSpawns);
		}
		else if (Now - HoldStartTime >= Settings->MaxHoldSeconds)
		{
			EndHold();

			// From build-up the peak is moved as well, after the rest of build-up, so no enemy is left behind
			// in a phase that will not run again.
			if (WavePhase == ENightlightWavePhase::BuildUp)
			{
				UNightlightWaveMath::MovePhaseEnemiesToFront(PeakEnemies, ReliefEnemies);
			}
			UNightlightWaveMath::MovePhaseEnemiesToFront(*GetPhaseEnemies(WavePhase), ReliefEnemies);

			bReliefCameEarly = true;
			OnChallengeReaction.Broadcast(ENightlightChallengeReaction::ReliefEarly);
			StartPhase(ENightlightWavePhase::Relief);
		}
		return;
	}

	if (ChallengeLevel > Settings->HighChallengeThreshold && HasEnemiesToSpawn())
	{
		HoldSpawns();
		OnChallengeReaction.Broadcast(ENightlightChallengeReaction::HeldSpawns);
		return;
	}

	// Only once a full window of this wave has been measured, because every wave starts at 0 before its
	// first enemies have reached the defence.
	if (WavePhase == ENightlightWavePhase::BuildUp
		&& ChallengeLevel < Settings->LowChallengeThreshold
		&& Now - WaveStartTime >= Settings->ChallengeWindowSeconds)
	{
		UNightlightWaveMath::MovePhaseEnemiesToFront(BuildUpEnemies, PeakEnemies);
		bPeakCameEarly = true;
		OnChallengeReaction.Broadcast(ENightlightChallengeReaction::PeakEarly);
		StartPhase(ENightlightWavePhase::Peak);
	}
}

void ANightlightWaveDirector::HoldSpawns()
{
	// A paused timer keeps its remaining time, so the next spawn is only delayed, never skipped
	// (Epic Games, Inc., 2026d).
	GetWorldTimerManager().PauseTimer(SpawnTimerHandle);
	bSpawnsHeld = true;
	HoldStartTime = GetWorld()->GetTimeSeconds();
}

void ANightlightWaveDirector::EndHold()
{
	if (!bSpawnsHeld)
	{
		return;
	}

	// Added to the clear-time target, so the player is not marked down for the time spawns were held.
	HeldSpawnSeconds += GetWorld()->GetTimeSeconds() - HoldStartTime;
	bSpawnsHeld = false;
}

void ANightlightWaveDirector::HandleEnemyRemoved(ANightlightEnemy* const Enemy)
{
	// Only this wave's enemies count, and nothing new starts while the level is closing.
	if (!Enemy || WaveEnemies.Remove(Enemy) == 0 || GetWorld()->bIsTearingDown)
	{
		return;
	}

	// Stopped means it died before reaching the Core. A Shade that dies while shooting the Core has reached it.
	if (Enemy->IsDead() && !Enemy->HasReachedCore())
	{
		++EnemiesStoppedThisWave;
	}

	BroadcastEnemiesRemaining();
	TryFinishWave();
}

void ANightlightWaveDirector::HandleCoreDestroyed()
{
	StopWaves();
}

void ANightlightWaveDirector::HandleCoreHealthChanged(const float CurrentHealth, const float MaxHealth)
{
	// The Core passes its new health, so the drop is worked out from the last value it sent.
	const float HealthDrop = LastKnownCoreHealth - CurrentHealth;
	LastKnownCoreHealth = CurrentHealth;

	if (HealthDrop > 0.0f && MaxHealth > 0.0f && IsWaveRunning())
	{
		CoreDamageTimes.Add(GetWorld()->GetTimeSeconds());
		CoreDamageFractions.Add(HealthDrop / MaxHealth);
	}
}

void ANightlightWaveDirector::HandleDefenderRemoved(ANightlightDefender* const Defender)
{
	// Only defenders that died count as lost. One that is sold or swapped for another type leaves alive.
	if (Defender && Defender->IsDead() && IsWaveRunning() && !GetWorld()->bIsTearingDown)
	{
		DefenderLostTimes.Add(GetWorld()->GetTimeSeconds());
	}
}

/*
References

Booth, M., 2009. The AI systems of Left 4 Dead. [pdf] Bellevue: Valve Corporation. Available at:
<https://cdn.fastly.steamstatic.com/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf> [Accessed 30 September 2026].

Epic Games, Inc., 2026a. Array Containers in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/array-containers-in-unreal-engine>
[Accessed 1 October 2026].

Epic Games, Inc., 2026b. Dynamic Delegates in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/dynamic-delegates-in-unreal-engine>
[Accessed 1 October 2026].

Epic Games, Inc., 2026c. FRandomStream. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FRandomStream>
[Accessed 1 October 2026].

Epic Games, Inc., 2026d. FTimerManager. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FTimerManager>
[Accessed 1 October 2026].

Epic Games, Inc., 2026e. Gameplay Timers in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-timers-in-unreal-engine>
[Accessed 1 October 2026].

Epic Games, Inc., 2026f. Programming Subsystems in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-subsystems-in-unreal-engine>
[Accessed 1 October 2026].

Epic Games, Inc., 2026g. TWeakObjectPtr. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/TWeakObjectPtr>
[Accessed 1 October 2026].

Epic Games, Inc., 2026h. UFunctions in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/ufunctions-in-unreal-engine>
[Accessed 1 October 2026].

Hunicke, R. and Chapman, V., 2004. AI for dynamic difficulty adjustment in games. [pdf] Evanston: Northwestern University. Available at:
<https://users.cs.northwestern.edu/~hunicke/pubs/Hamlet.pdf> [Accessed 30 September 2026].
*/
