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
	// keeps the order the same for the same seed (Epic Games, Inc., 2026c).
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
	// after that (Epic Games, Inc., 2026d).
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
		}

		// The same weighted pick the spawn routes use, so there is only one implementation of it.
		const int32 PickedSlot = PickWeightedIndex(CandidateWeights, RandomStream);
		if (PickedSlot == INDEX_NONE)
		{
			break;
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
	// depends on the plan and the seed (Epic Games, Inc., 2026e).
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

	// Full marks up to the target, then a straight line down to 0 at twice the target (Epic Games, Inc., 2026d).
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

int32 UNightlightWaveMath::PickWeightedIndex(const TArray<float>& Weights, FRandomStream& RandomStream)
{
	float TotalWeight = 0.0f;
	for (const float Weight : Weights)
	{
		TotalWeight += FMath::Max(Weight, 0.0f);
	}

	// Roll a point along the total weight and walk the slices until the roll is used up. Zero weights have no
	// slice, so they are never picked. The stream gives the same picks for the same seed (Epic Games, Inc., 2026c).
	float Roll = RandomStream.FRand() * TotalWeight;
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		if (Weights[Index] > 0.0f && (Roll -= Weights[Index]) < 0.0f)
		{
			return Index;
		}
	}

	// Float rounding can leave a tiny roll over, so it falls to the last slice. -1 when nothing can be picked.
	return Weights.FindLastByPredicate([](const float Weight) { return Weight > 0.0f; });
}

float UNightlightWaveMath::CalculateRouteDefence(
	const TArray<FVector>& RoutePoints,
	const TArray<FVector>& DefenderLocations,
	const TArray<float>& DefenderRanges,
	const TArray<float>& DefenderDps)
{
	float Defence = 0.0f;
	const int32 DefenderCount = FMath::Min3(DefenderLocations.Num(), DefenderRanges.Num(), DefenderDps.Num());
	for (int32 DefenderIndex = 0; DefenderIndex < DefenderCount; ++DefenderIndex)
	{
		// Each segment runs from the previous point to this one, and a defender counts once however many it reaches.
		// The closest point on the segment, not just its ends, catches a defender beside a long straight
		// (Epic Games, Inc., 2026b).
		for (int32 PointIndex = 0; PointIndex < RoutePoints.Num(); ++PointIndex)
		{
			const FVector ClosestPoint = FMath::ClosestPointOnSegment(
				DefenderLocations[DefenderIndex], RoutePoints[FMath::Max(PointIndex - 1, 0)], RoutePoints[PointIndex]);
			if (FVector::Dist(DefenderLocations[DefenderIndex], ClosestPoint) <= DefenderRanges[DefenderIndex])
			{
				Defence += FMath::Max(DefenderDps[DefenderIndex], 0.0f);
				break;
			}
		}
	}
	return Defence;
}

float UNightlightWaveMath::CalculateRouteWeight(const UNightlightWaveSettings* Settings, const float Defence, FRandomStream& RandomStream)
{
	Settings = GetSettingsOrDefault(Settings);

	// RouteDefenceScale damage per second halves the weight. The random factor stops the least defended route
	// always winning (Epic Games, Inc., 2026c).
	const float DefenceWeight = 1.0f / (1.0f + FMath::Max(Defence, 0.0f) / Settings->RouteDefenceScale);
	return DefenceWeight * RandomStream.FRandRange(1.0f - Settings->RouteRandomFactor, 1.0f + Settings->RouteRandomFactor);
}

float UNightlightWaveMath::CalculateRouteLength(const TArray<FVector>& RoutePoints)
{
	float Length = 0.0f;
	for (int32 PointIndex = 1; PointIndex < RoutePoints.Num(); ++PointIndex)
	{
		Length += FVector::Dist(RoutePoints[PointIndex - 1], RoutePoints[PointIndex]);
	}
	return Length;
}

TArray<float> UNightlightWaveMath::ApplyRoleRoutePreference(
	const UNightlightWaveSettings* Settings,
	const ENightlightWaveEnemyRole Role,
	const TArray<float>& BaseWeights,
	const TArray<float>& RouteLengths)
{
	Settings = GetSettingsOrDefault(Settings);
	TArray<float> Weights = BaseWeights;
	if (Role == ENightlightWaveEnemyRole::Walker || Weights.Num() < 2 || RouteLengths.Num() != Weights.Num())
	{
		return Weights;
	}

	// JTM1512's idea as a multiplier on the defence weight, not a hard rule, so a well defended short route can
	// still lose to a bare long one.
	const bool bBrute = Role == ENightlightWaveEnemyRole::Brute;
	int32 PreferredIndex = 0;
	for (int32 RouteIndex = 1; RouteIndex < RouteLengths.Num(); ++RouteIndex)
	{
		if (bBrute ? RouteLengths[RouteIndex] < RouteLengths[PreferredIndex] : RouteLengths[RouteIndex] > RouteLengths[PreferredIndex])
		{
			PreferredIndex = RouteIndex;
		}
	}

	Weights[PreferredIndex] *= bBrute ? Settings->BruteShortestRouteMultiplier : Settings->ShadeLongestRouteMultiplier;
	return Weights;
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

Epic Games, Inc., 2026b. FMath::ClosestPointOnSegment. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FMath/ClosestPointOnSegment>
[Accessed 1 October 2026].

Epic Games, Inc., 2026c. FRandomStream. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FRandomStream>
[Accessed 1 October 2026].

Epic Games, Inc., 2026d. GetMappedRangeValueClamped. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FMath/GetMappedRangeValueClamped>
[Accessed 1 October 2026].

Epic Games, Inc., 2026e. TArray::StableSort. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/TArray/StableSort>
[Accessed 1 October 2026].

Epic Games, Inc., 2026f. UBlueprintFunctionLibrary. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UBlueprintFunctionLibrary>
[Accessed 1 October 2026].

Hunicke, R. and Chapman, V., 2004. AI for dynamic difficulty adjustment in games. [pdf] Evanston: Northwestern University. Available at:
<https://users.cs.northwestern.edu/~hunicke/pubs/Hamlet.pdf> [Accessed 30 September 2026].
*/
