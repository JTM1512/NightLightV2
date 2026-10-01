#include "NightlightWaveMath.h"
#include "../Enemies/NightlightEnemy.h"

namespace
{
	// Falls back to the class defaults, which match the planning document, when no asset is assigned.
	const UNightlightWaveSettings* GetSettingsOrDefault(const UNightlightWaveSettings* const Settings)
	{
		return Settings ? Settings : GetDefault<UNightlightWaveSettings>();
	}

	int32 GetThreatCost(const UNightlightWaveSettings* const Settings, const int32 EntryIndex)
	{
		return Settings->Enemies.IsValidIndex(EntryIndex) ? Settings->Enemies[EntryIndex].ThreatCost : 0;
	}

	// Fisher-Yates shuffle: swap each slot, from the back, with a random slot at or before it. The stream
	// keeps the order the same for the same seed (Epic Games, Inc., 2026b).
	void ShuffleWithStream(TArray<int32>& Entries, FRandomStream& RandomStream)
	{
		for (int32 Index = Entries.Num() - 1; Index > 0; --Index)
		{
			Entries.Swap(Index, RandomStream.RandRange(0, Index));
		}
	}
}

float FNightlightEnemyMixWeights::GetWeightForRole(const ENightlightWaveEnemyRole Role) const
{
	switch (Role)
	{
	case ENightlightWaveEnemyRole::Shade:
		return Shade;
	case ENightlightWaveEnemyRole::Brute:
		return Brute;
	default:
		return Walker;
	}
}

bool UNightlightWaveMath::IsBruteWave(const UNightlightWaveSettings* Settings, const int32 WaveNumber)
{
	Settings = GetSettingsOrDefault(Settings);
	if (WaveNumber < 1 || Settings->BruteWaveEvery < 1)
	{
		return false;
	}

	// A peak every few waves, with a calmer wave after it (Booth, 2009).
	return WaveNumber % Settings->BruteWaveEvery == 0;
}

int32 UNightlightWaveMath::CalculateWaveBudget(
	const UNightlightWaveSettings* Settings,
	const int32 WaveNumber,
	const float AdaptiveFactor)
{
	Settings = GetSettingsOrDefault(Settings);
	const int32 SafeWaveNumber = FMath::Max(WaveNumber, 1);

	// Linear growth, not exponential, so each wave is only a small step harder than the last (Chen, 2007).
	float Budget = (Settings->StartingBudget + Settings->BudgetGrowthPerWave * SafeWaveNumber) * AdaptiveFactor;

	if (IsBruteWave(Settings, SafeWaveNumber))
	{
		Budget *= Settings->BruteWaveBudgetMultiplier;
	}

	return FMath::Max(FMath::RoundToInt(Budget), 1);
}

float UNightlightWaveMath::CalculateSpawnGap(const UNightlightWaveSettings* Settings, const int32 WaveNumber)
{
	Settings = GetSettingsOrDefault(Settings);
	if (Settings->SpawnGapRampWaves <= 1)
	{
		return Settings->FinalSpawnGap;
	}

	// Maps wave 1 to the first gap and the last ramp wave to the final gap. The clamp keeps it flat
	// after that (Epic Games, Inc., 2026c).
	return FMath::GetMappedRangeValueClamped(
		FVector2f(1.0f, static_cast<float>(Settings->SpawnGapRampWaves)),
		FVector2f(Settings->FirstWaveSpawnGap, Settings->FinalSpawnGap),
		static_cast<float>(WaveNumber));
}

TArray<int32> UNightlightWaveMath::PlanWaveEnemies(
	const UNightlightWaveSettings* Settings,
	const int32 WaveNumber,
	const int32 Budget,
	const FNightlightEnemyMixWeights& TypeWeightMultipliers,
	FRandomStream& RandomStream)
{
	Settings = GetSettingsOrDefault(Settings);
	const bool bBruteWave = IsBruteWave(Settings, WaveNumber);

	TArray<int32> PlannedEntries;
	TArray<int32> CandidateIndices;
	TArray<float> CandidateWeights;
	int32 RemainingBudget = Budget;

	while (RemainingBudget > 0)
	{
		CandidateIndices.Reset();
		CandidateWeights.Reset();
		float TotalWeight = 0.0f;

		// Only unlocked types with a class that the remaining budget can still pay for.
		for (int32 EntryIndex = 0; EntryIndex < Settings->Enemies.Num(); ++EntryIndex)
		{
			const FNightlightWaveEnemyEntry& Entry = Settings->Enemies[EntryIndex];
			if (!Entry.EnemyClass
				|| WaveNumber < Entry.UnlockWave
				|| Entry.ThreatCost < 1
				|| Entry.ThreatCost > RemainingBudget)
			{
				continue;
			}

			float Weight = Entry.BaseWeight * TypeWeightMultipliers.GetWeightForRole(Entry.Role);
			if (bBruteWave && Entry.Role == ENightlightWaveEnemyRole::Brute)
			{
				Weight *= Settings->BruteWaveBruteWeightMultiplier;
			}

			if (Weight <= 0.0f)
			{
				continue;
			}

			CandidateIndices.Add(EntryIndex);
			CandidateWeights.Add(Weight);
			TotalWeight += Weight;
		}

		if (CandidateIndices.IsEmpty())
		{
			break;
		}

		// Weighted random pick: roll a point along the total weight and find whose slice it lands in. The
		// stream makes the same seed give the same wave every time (Epic Games, Inc., 2026b).
		const float Roll = RandomStream.FRand() * TotalWeight;
		int32 PickedSlot = CandidateIndices.Num() - 1;
		float RunningWeight = 0.0f;
		for (int32 Slot = 0; Slot < CandidateIndices.Num(); ++Slot)
		{
			RunningWeight += CandidateWeights[Slot];
			if (Roll < RunningWeight)
			{
				PickedSlot = Slot;
				break;
			}
		}

		const int32 PickedEntryIndex = CandidateIndices[PickedSlot];
		PlannedEntries.Add(PickedEntryIndex);
		RemainingBudget -= Settings->Enemies[PickedEntryIndex].ThreatCost;
	}

	return PlannedEntries;
}

FNightlightWavePhaseLists UNightlightWaveMath::SplitIntoPhases(
	const UNightlightWaveSettings* Settings,
	const TArray<int32>& PlannedEntryIndices,
	FRandomStream& RandomStream)
{
	Settings = GetSettingsOrDefault(Settings);
	FNightlightWavePhaseLists PhaseLists;

	// JTM1512's idea: each wave builds up, peaks, then gives the player a moment of relief.
	const int32 EnemyCount = PlannedEntryIndices.Num();
	const int32 BuildUpCount = FMath::Clamp(FMath::RoundToInt(EnemyCount * Settings->BuildUpShare), 0, EnemyCount);
	const int32 PeakCount = FMath::Clamp(FMath::RoundToInt(EnemyCount * Settings->PeakShare), 0, EnemyCount - BuildUpCount);

	// Most expensive first. A stable sort keeps equal costs in their planned order, so the split only
	// depends on the plan and the seed (Epic Games, Inc., 2026d).
	TArray<int32> SortedEntries = PlannedEntryIndices;
	SortedEntries.StableSort([Settings](const int32 First, const int32 Second)
	{
		return GetThreatCost(Settings, First) > GetThreatCost(Settings, Second);
	});

	// The peak takes the front of the list, build-up the cheapest at the back and relief the middle.
	const int32 BuildUpStart = EnemyCount - BuildUpCount;
	for (int32 SortedIndex = 0; SortedIndex < EnemyCount; ++SortedIndex)
	{
		if (SortedIndex < PeakCount)
		{
			PhaseLists.Peak.Add(SortedEntries[SortedIndex]);
		}
		else if (SortedIndex < BuildUpStart)
		{
			PhaseLists.Relief.Add(SortedEntries[SortedIndex]);
		}
		else
		{
			PhaseLists.BuildUp.Add(SortedEntries[SortedIndex]);
		}
	}

	// Shuffled so a phase does not always spawn its types in the same fixed order.
	ShuffleWithStream(PhaseLists.BuildUp, RandomStream);
	ShuffleWithStream(PhaseLists.Peak, RandomStream);
	ShuffleWithStream(PhaseLists.Relief, RandomStream);
	return PhaseLists;
}

float UNightlightWaveMath::GetPhaseSpawnGap(
	const UNightlightWaveSettings* Settings,
	const int32 WaveNumber,
	const ENightlightWavePhase Phase)
{
	Settings = GetSettingsOrDefault(Settings);
	const float PeakGap = CalculateSpawnGap(Settings, WaveNumber);
	return Phase == ENightlightWavePhase::Peak ? PeakGap : PeakGap * Settings->OffPeakGapMultiplier;
}

float UNightlightWaveMath::CalculatePlannedSpawnSeconds(
	const UNightlightWaveSettings* Settings,
	const int32 WaveNumber,
	const FNightlightWavePhaseLists& PhaseLists)
{
	Settings = GetSettingsOrDefault(Settings);
	float Seconds = 0.0f;
	float LastGap = 0.0f;

	const TPair<const TArray<int32>*, ENightlightWavePhase> Phases[] = {
		{ &PhaseLists.BuildUp, ENightlightWavePhase::BuildUp },
		{ &PhaseLists.Peak, ENightlightWavePhase::Peak },
		{ &PhaseLists.Relief, ENightlightWavePhase::Relief } };

	for (const TPair<const TArray<int32>*, ENightlightWavePhase>& Phase : Phases)
	{
		if (!Phase.Key->IsEmpty())
		{
			LastGap = GetPhaseSpawnGap(Settings, WaveNumber, Phase.Value);
			Seconds += Phase.Key->Num() * LastGap;
		}
	}

	// Nothing waits after the very last enemy, so its gap is taken off again.
	return FMath::Max(Seconds - LastGap, 0.0f);
}

float UNightlightWaveMath::CalculateClearTimeScore(
	const UNightlightWaveSettings* Settings,
	const float ClearSeconds,
	const float TargetSpawnSeconds)
{
	Settings = GetSettingsOrDefault(Settings);
	const float TargetSeconds = FMath::Max(TargetSpawnSeconds, 0.0f) + Settings->ClearTimeGraceSeconds;
	if (TargetSeconds <= 0.0f)
	{
		return 1.0f;
	}

	// Full marks up to the target, then a straight line down to 0 at twice the target (Epic Games, Inc., 2026c).
	return FMath::GetMappedRangeValueClamped(
		FVector2f(TargetSeconds, TargetSeconds * 2.0f),
		FVector2f(1.0f, 0.0f),
		ClearSeconds);
}

float UNightlightWaveMath::CalculateUnspentTokensScore(const UNightlightWaveSettings* Settings, const int32 UnspentTokens)
{
	Settings = GetSettingsOrDefault(Settings);
	if (UnspentTokens < 0)
	{
		return -1.0f;
	}

	const float Score = FMath::Clamp(static_cast<float>(UnspentTokens) / FMath::Max(Settings->TokensForFullScore, 1), 0.0f, 1.0f);
	return Settings->bSpareTokensRaiseScore ? Score : 1.0f - Score;
}

float UNightlightWaveMath::CalculatePlayerScore(const UNightlightWaveSettings* Settings, const FNightlightWaveMeasures& Measures)
{
	Settings = GetSettingsOrDefault(Settings);

	// Each measure with its weight. Unavailable measures are skipped and the rest are divided by their own
	// total weight, so leaving one out does not drag the score down.
	const TPair<float, float> WeightedMeasures[] = {
		{ Measures.CoreHealthKept, Settings->CoreHealthWeight },
		{ Measures.EnemiesStopped, Settings->EnemiesStoppedWeight },
		{ Measures.ClearTime, Settings->ClearTimeWeight },
		{ Measures.UnspentTokens, Settings->UnspentTokensWeight } };

	float WeightedTotal = 0.0f;
	float TotalWeight = 0.0f;
	for (const TPair<float, float>& Measure : WeightedMeasures)
	{
		if (Measure.Key >= 0.0f && Measure.Value > 0.0f)
		{
			WeightedTotal += FMath::Clamp(Measure.Key, 0.0f, 1.0f) * Measure.Value;
			TotalWeight += Measure.Value;
		}
	}

	// Halfway between the thresholds, so a wave with nothing to measure never changes A.
	return TotalWeight > 0.0f
		? WeightedTotal / TotalWeight
		: (Settings->RaiseScoreThreshold + Settings->LowerScoreThreshold) * 0.5f;
}

float UNightlightWaveMath::UpdateAdaptiveFactor(
	const UNightlightWaveSettings* Settings,
	const float CurrentAdaptiveFactor,
	const float PlayerScore)
{
	Settings = GetSettingsOrDefault(Settings);
	float NewAdaptiveFactor = CurrentAdaptiveFactor;

	// One small step at a time, and only between waves, so the player does not notice the game adjusting
	// to them (Hunicke and Chapman, 2004).
	if (PlayerScore > Settings->RaiseScoreThreshold)
	{
		NewAdaptiveFactor += Settings->AdaptiveStep;
	}
	else if (PlayerScore < Settings->LowerScoreThreshold)
	{
		NewAdaptiveFactor -= Settings->AdaptiveStep;
	}

	return FMath::Clamp(NewAdaptiveFactor, Settings->MinAdaptiveFactor, Settings->MaxAdaptiveFactor);
}

float UNightlightWaveMath::CalculateChallengeLevel(
	const UNightlightWaveSettings* Settings,
	const float CoreDamageFraction,
	const int32 DefendersLost,
	const int32 EnemiesNearCore)
{
	Settings = GetSettingsOrDefault(Settings);

	// Each input as a share of its "full pressure" value, so one large input cannot outweigh the others.
	const TPair<float, float> WeightedInputs[] = {
		{ CoreDamageFraction / FMath::Max(Settings->CoreDamageForFullChallenge, KINDA_SMALL_NUMBER), Settings->CoreDamageChallengeWeight },
		{ static_cast<float>(DefendersLost) / FMath::Max(Settings->DefendersLostForFullChallenge, 1), Settings->DefendersLostChallengeWeight },
		{ static_cast<float>(EnemiesNearCore) / FMath::Max(Settings->EnemiesNearCoreForFullChallenge, 1), Settings->EnemiesNearCoreChallengeWeight } };

	float WeightedTotal = 0.0f;
	float TotalWeight = 0.0f;
	for (const TPair<float, float>& Input : WeightedInputs)
	{
		if (Input.Value > 0.0f)
		{
			WeightedTotal += FMath::Clamp(Input.Key, 0.0f, 1.0f) * Input.Value;
			TotalWeight += Input.Value;
		}
	}

	return TotalWeight > 0.0f ? WeightedTotal / TotalWeight * 100.0f : 0.0f;
}

void UNightlightWaveMath::MovePhaseEnemiesToFront(TArray<int32>& FromPhase, TArray<int32>& ToPhase)
{
	// Inserting the whole list at index 0 keeps both orders, so the moved enemies spawn next (Epic Games, Inc., 2026a).
	ToPhase.Insert(FromPhase, 0);
	FromPhase.Reset();
}

/*
References

Booth, M., 2009. The AI systems of Left 4 Dead. [pdf] Bellevue: Valve Corporation. Available at:
<https://cdn.fastly.steamstatic.com/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf> [Accessed 30 September 2026].

Chen, J., 2007. Flow in games (and everything else). Communications of the ACM, [e-journal] 50(4), pp.31-34. Available at:
<https://khoury.northeastern.edu/~lieber/courses/csu670/f08/materials/p31-chen-flow-in-games.pdf> [Accessed 30 September 2026].

Epic Games, Inc., 2026a. Array Containers in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/array-containers-in-unreal-engine>
[Accessed 1 October 2026].

Epic Games, Inc., 2026b. FRandomStream. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FRandomStream>
[Accessed 1 October 2026].

Epic Games, Inc., 2026c. GetMappedRangeValueClamped. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FMath/GetMappedRangeValueClamped>
[Accessed 1 October 2026].

Epic Games, Inc., 2026d. TArray::StableSort. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/TArray/StableSort>
[Accessed 1 October 2026].

Epic Games, Inc., 2026e. UBlueprintFunctionLibrary. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UBlueprintFunctionLibrary>
[Accessed 1 October 2026].

Hunicke, R. and Chapman, V., 2004. AI for dynamic difficulty adjustment in games. [pdf] Evanston: Northwestern University. Available at:
<https://users.cs.northwestern.edu/~hunicke/pubs/Hamlet.pdf> [Accessed 30 September 2026].
*/
