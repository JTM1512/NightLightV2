#include "NightlightEliteGenerator.h"
#include "../Waves/NightlightWaveSettings.h"

namespace
{
	// Falls back to the class defaults, which match the planning document, when no asset is assigned.
	const UNightlightWaveSettings* GetEliteSettingsOrDefault(const UNightlightWaveSettings* const Settings)
	{
		return Settings ? Settings : GetDefault<UNightlightWaveSettings>();
	}

	bool CountersPlayStyle(const ENightlightWaveTemplateCounter Counters, const FNightlightPlayStyle& PlayStyle)
	{
		return (Counters == ENightlightWaveTemplateCounter::ShortRange && PlayStyle.bMostlyShortRange)
			|| (Counters == ENightlightWaveTemplateCounter::LongRange && PlayStyle.bMostlyLongRange)
			|| (Counters == ENightlightWaveTemplateCounter::Packed && PlayStyle.bPackedTogether);
	}

	// True when either rule lists the other as excluded, so a pair only needs to be set up on one side.
	bool AreExcluded(const FNightlightEliteMutationRule& First, const FNightlightEliteMutationRule& Second)
	{
		return First.ExcludedMutations.Contains(Second.Mutation) || Second.ExcludedMutations.Contains(First.Mutation);
	}
}

float UNightlightEliteGenerator::CalculateEliteChance(const UNightlightWaveSettings* Settings, const int32 WaveNumber)
{
	Settings = GetEliteSettingsOrDefault(Settings);
	if (WaveNumber < Settings->EliteStartWave)
	{
		return 0.0f;
	}

	// A straight line from the first elite wave, capped so most of every wave stays normal (Chen, 2007).
	const float Chance = Settings->EliteBaseChance + Settings->EliteChancePerWave * (WaveNumber - Settings->EliteStartWave);
	return FMath::Clamp(Chance, 0.0f, FMath::Clamp(Settings->EliteMaxChance, 0.0f, 1.0f));
}

int32 UNightlightEliteGenerator::GetMutationCount(const UNightlightWaveSettings* Settings, const int32 WaveNumber)
{
	Settings = GetEliteSettingsOrDefault(Settings);
	return WaveNumber >= Settings->TwoMutationWave ? 2 : 1;
}

int32 UNightlightEliteGenerator::MakeEliteSeed(const int32 MapSeed, const int32 WaveNumber, const int32 PlanIndex)
{
	// HashCombine mixes the three numbers so that neighbouring waves or plan slots do not give neighbouring seeds,
	// while the same three numbers always give the same seed (Epic Games, Inc., 2026b).
	const uint32 Hash = HashCombine(HashCombine(GetTypeHash(MapSeed), GetTypeHash(WaveNumber)), GetTypeHash(PlanIndex));
	return static_cast<int32>(Hash);
}

float UNightlightEliteGenerator::GetMutationWeight(
	const UNightlightWaveSettings* Settings,
	const FNightlightEliteMutationRule& Rule,
	const FNightlightPlayStyle& PlayStyle)
{
	Settings = GetEliteSettingsOrDefault(Settings);

	// The Part 2 play-style reading makes the mutation that punishes the player's build more likely, the same way
	// the wave templates counter it (Hunicke and Chapman, 2004).
	const float Weight = FMath::Max(Rule.Weight, 0.0f);
	return CountersPlayStyle(Rule.Counters, PlayStyle) ? Weight * FMath::Max(Settings->EliteCounterWeightMultiplier, 1.0f) : Weight;
}

FNightlightEliteProfile UNightlightEliteGenerator::GenerateEliteProfile(
	const UNightlightWaveSettings* Settings,
	const int32 WaveNumber,
	const int32 Seed,
	const FNightlightPlayStyle& PlayStyle,
	const ENightlightWaveEnemyRole BaseRole)
{
	Settings = GetEliteSettingsOrDefault(Settings);

	FNightlightEliteProfile Profile;
	Profile.bIsElite = true;
	Profile.Seed = Seed;
	Profile.BaseRole = BaseRole;
	Profile.FrenzyHealthFraction = Settings->FrenzyHealthFraction;

	// Each elite has its own stream, so its rolls never depend on how many random numbers the wave used before it
	// (Epic Games, Inc., 2026a).
	FRandomStream Stream(Seed);
	const int32 MutationCount = GetMutationCount(Settings, WaveNumber);
	TArray<const FNightlightEliteMutationRule*> PickedRules;
	TArray<float> Weights;

	for (int32 Pick = 0; Pick < MutationCount; ++Pick)
	{
		// A rule already picked, a repeat of a picked mutation or one excluded by a picked rule gets no weight, so
		// the weighted pick never chooses it.
		Weights.Reset();
		for (const FNightlightEliteMutationRule& Rule : Settings->EliteMutations)
		{
			const bool bBlocked = PickedRules.ContainsByPredicate([&Rule](const FNightlightEliteMutationRule* const Picked)
			{
				return Picked->Mutation == Rule.Mutation || AreExcluded(*Picked, Rule);
			});
			Weights.Add(bBlocked ? 0.0f : GetMutationWeight(Settings, Rule, PlayStyle));
		}

		// The same weighted pick the waves and routes use, so there is only one implementation of it.
		const int32 PickedIndex = UNightlightWaveMath::PickWeightedIndex(Weights, Stream);
		if (PickedIndex == INDEX_NONE)
		{
			break;
		}

		PickedRules.Add(&Settings->EliteMutations[PickedIndex]);
		Profile.Mutations.Add(Settings->EliteMutations[PickedIndex].Mutation);
		Profile.MutationStrengths.Add(Stream.FRand());
	}

	// Combine the mutations multiplicatively, so two mutations that both add health stack, then clamp the result.
	float HealthMultiplier = 1.0f;
	float SpeedMultiplier = 1.0f;
	FVector Stretch = FVector::OneVector;
	FLinearColor TintTotal = FLinearColor::Black;
	float TintWeightTotal = 0.0f;
	for (int32 Index = 0; Index < PickedRules.Num(); ++Index)
	{
		const FNightlightEliteMutationRule& Rule = *PickedRules[Index];
		const float Strength = Profile.MutationStrengths[Index];

		HealthMultiplier *= FMath::Lerp(Rule.MinHealthMultiplier, Rule.MaxHealthMultiplier, Strength);
		SpeedMultiplier *= FMath::Lerp(Rule.MinSpeedMultiplier, Rule.MaxSpeedMultiplier, Strength);
		Profile.HurtAttackIntervalMultiplier *= FMath::Lerp(Rule.MinHurtAttackIntervalMultiplier, Rule.MaxHurtAttackIntervalMultiplier, Strength);
		Profile.HealFraction += FMath::Lerp(Rule.MinHealFraction, Rule.MaxHealFraction, Strength);
		Stretch *= FMath::Lerp(FVector::OneVector, Rule.StretchAtFullStrength, Strength);
		Profile.PulseSpeed = FMath::Max(Profile.PulseSpeed, Rule.PulseSpeed);

		// A stronger roll shows more of its colour. The floor keeps a weak roll visible in the blend.
		const float TintWeight = 0.25f + Strength;
		TintTotal += Rule.Colour * TintWeight;
		TintWeightTotal += TintWeight;
	}

	Profile.HealthMultiplier = FMath::Clamp(HealthMultiplier, 0.1f, FMath::Max(Settings->EliteMaxHealthMultiplier, 1.0f));
	Profile.SpeedMultiplier = FMath::Clamp(SpeedMultiplier, 0.1f, FMath::Max(Settings->EliteMaxSpeedMultiplier, 1.0f));
	Profile.HealFraction = FMath::Clamp(Profile.HealFraction, 0.0f, 1.0f);

	// The look follows the stats: a tougher elite is bigger, Swift stretches it, and more mutations glow brighter.
	const float SizeScale = FMath::Max(1.0f + (Profile.HealthMultiplier - 1.0f) * Settings->EliteScalePerExtraHealth, 0.1f);
	Profile.MeshScale = Stretch * SizeScale;
	Profile.Tint = TintWeightTotal > 0.0f ? TintTotal / TintWeightTotal : FLinearColor::White;
	Profile.Tint.A = 1.0f;
	Profile.Glow = Settings->EliteGlowPerMutation * Profile.Mutations.Num();
	Profile.TokenMultiplier = 1.0f + Settings->EliteTokenBonusPerMutation * Profile.Mutations.Num();
	Profile.DisplayName = BuildEliteDisplayName(Profile.Mutations, BaseRole);
	return Profile;
}

FText UNightlightEliteGenerator::BuildEliteDisplayName(
	const TArray<ENightlightEliteMutation>& Mutations,
	const ENightlightWaveEnemyRole BaseRole)
{
	// The enums' display names, so a renamed entry in the editor carries through to the HUD.
	TArray<FText> Words;
	for (const ENightlightEliteMutation Mutation : Mutations)
	{
		Words.Add(UEnum::GetDisplayValueAsText(Mutation));
	}
	Words.Add(UEnum::GetDisplayValueAsText(BaseRole));
	return FText::Join(FText::FromString(TEXT(" ")), Words);
}

int32 UNightlightEliteGenerator::CalculateEliteTokens(const int32 BaseTokens, const FNightlightEliteProfile& Profile)
{
	return Profile.bIsElite ? FMath::RoundToInt(FMath::Max(BaseTokens, 0) * FMath::Max(Profile.TokenMultiplier, 1.0f)) : BaseTokens;
}

/*
References

Chen, J., 2007. Flow in games (and everything else). Communications of the ACM, [e-journal] 50(4), pp.31-34. Available at:
<https://khoury.northeastern.edu/~lieber/courses/csu670/f08/materials/p31-chen-flow-in-games.pdf> [Accessed 30 September 2026].

Epic Games, Inc., 2026a. FRandomStream. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FRandomStream>
[Accessed 6 November 2026].

Epic Games, Inc., 2026b. HashCombine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/HashCombine>
[Accessed 6 November 2026].

Epic Games, Inc., 2026c. UBlueprintFunctionLibrary. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UBlueprintFunctionLibrary>
[Accessed 6 November 2026].

Hunicke, R. and Chapman, V., 2004. AI for dynamic difficulty adjustment in games. [pdf] Evanston: Northwestern University. Available at:
<https://users.cs.northwestern.edu/~hunicke/pubs/Hamlet.pdf> [Accessed 30 September 2026].
*/
