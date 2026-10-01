#include "NightlightWaveMath.h"
#include "../Enemies/NightlightEnemy.h"

namespace
{
	// Falls back to the class defaults, which match the planning document, when no asset is assigned.
	const UNightlightWaveSettings* GetSettingsOrDefault(const UNightlightWaveSettings* const Settings)
	{
		return Settings ? Settings : GetDefault<UNightlightWaveSettings>();
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
	// after that (Epic Games, Inc., 2026b).
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
		// stream makes the same seed give the same wave every time (Epic Games, Inc., 2026a).
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

/*
References

Booth, M., 2009. The AI systems of Left 4 Dead. [pdf] Bellevue: Valve Corporation. Available at:
<https://cdn.fastly.steamstatic.com/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf> [Accessed 30 September 2026].

Chen, J., 2007. Flow in games (and everything else). Communications of the ACM, [e-journal] 50(4), pp.31-34. Available at:
<https://khoury.northeastern.edu/~lieber/courses/csu670/f08/materials/p31-chen-flow-in-games.pdf> [Accessed 30 September 2026].

Epic Games, Inc., 2026a. FRandomStream. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FRandomStream>
[Accessed 1 October 2026].

Epic Games, Inc., 2026b. GetMappedRangeValueClamped. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FMath/GetMappedRangeValueClamped>
[Accessed 1 October 2026].

Epic Games, Inc., 2026c. UBlueprintFunctionLibrary. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UBlueprintFunctionLibrary>
[Accessed 1 October 2026].
*/
