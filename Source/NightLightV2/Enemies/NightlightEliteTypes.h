#pragma once

#include "CoreMinimal.h"
#include "../Waves/NightlightWaveTypes.h"
#include "NightlightEliteTypes.generated.h"

// One change an elite can roll. An elite gets one of these up to wave 9 and two from wave 10.
UENUM(BlueprintType)
enum class ENightlightEliteMutation : uint8
{
	// Faster but a little more fragile. Counters long-range builds, which rely on time to shoot.
	Swift,

	// Tougher but slower. Counters short-range builds, which rely on burst damage up close.
	Armoured,

	// Attacks faster once it drops below half health.
	Frenzied,

	// Heals from the damage it deals to defenders. Counters packed builds, where it always has something to hit.
	Siphon
};

// The tuning for one mutation. Every range is rolled with the same strength from 0 to 1, so a strong roll is
// strong in every stat the mutation changes (Epic Games, Inc., 2026).
USTRUCT(BlueprintType)
struct NIGHTLIGHTV2_API FNightlightEliteMutationRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	ENightlightEliteMutation Mutation = ENightlightEliteMutation::Swift;

	// How likely this mutation is to be picked compared with the others. Zero removes it from the pool.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	// Mutations that can never be rolled together with this one, such as Swift and Armoured.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	TArray<ENightlightEliteMutation> ExcludedMutations;

	// The play style this mutation counters. Its weight is multiplied by EliteCounterWeightMultiplier in the wave
	// settings when the player has built that way. Uses the same values as the wave template counters.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	ENightlightWaveTemplateCounter Counters = ENightlightWaveTemplateCounter::None;

	// Max health multiplier at strength 0 and at strength 1.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite", meta = (ClampMin = "0.1"))
	float MinHealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite", meta = (ClampMin = "0.1"))
	float MaxHealthMultiplier = 1.0f;

	// Movement speed multiplier at strength 0 and at strength 1.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite", meta = (ClampMin = "0.1"))
	float MinSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite", meta = (ClampMin = "0.1"))
	float MaxSpeedMultiplier = 1.0f;

	// Defender attack interval multiplier once the elite is hurt below the frenzy threshold. Below 1 attacks faster.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite", meta = (ClampMin = "0.1"))
	float MinHurtAttackIntervalMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite", meta = (ClampMin = "0.1"))
	float MaxHurtAttackIntervalMultiplier = 1.0f;

	// The share of the damage dealt to defenders that the elite heals back.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinHealFraction = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxHealFraction = 0.0f;

	// The colour this mutation adds to the elite's tint and glow.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	FLinearColor Colour = FLinearColor::White;

	// Mesh scale multiplier at strength 1, on top of the size that follows health. Swift is taller and thinner.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	FVector StretchAtFullStrength = FVector::OneVector;

	// Glow pulses per second. Zero holds the glow steady. The pulse speeds up as the elite is hurt.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite", meta = (ClampMin = "0.0"))
	float PulseSpeed = 0.0f;
};

// Everything one elite rolled, worked out before it spawns so the same seed always gives the same elite.
// A default profile is a normal enemy.
USTRUCT(BlueprintType)
struct NIGHTLIGHTV2_API FNightlightEliteProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	bool bIsElite = false;

	// The seed the profile was rolled from, built from the map seed, the wave and the enemy's place in the plan.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	int32 Seed = 0;

	// The enemy type the elite was made from, used for its name.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	ENightlightWaveEnemyRole BaseRole = ENightlightWaveEnemyRole::Walker;

	// In the order they were picked, never repeated.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	TArray<ENightlightEliteMutation> Mutations;

	// Each mutation's rolled strength from 0 to 1, lined up with Mutations by index.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	TArray<float> MutationStrengths;

	// The mutations multiplied together, then clamped to the wave settings' maximums.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	float HealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	float SpeedMultiplier = 1.0f;

	// Applied to DefenderAttackInterval once health drops below FrenzyHealthFraction. 1 when not Frenzied.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	float HurtAttackIntervalMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	float FrenzyHealthFraction = 0.5f;

	// The share of damage dealt to defenders healed back. 0 when not Siphon.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	float HealFraction = 0.0f;

	// The mesh scale multiplier: larger with more health, stretched by Swift.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	FVector MeshScale = FVector::OneVector;

	// Each mutation's colour blended by its strength.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	FLinearColor Tint = FLinearColor::White;

	// Rises with the number of mutations.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	float Glow = 0.0f;

	// Pulses per second at full health. 0 holds the glow steady.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	float PulseSpeed = 0.0f;

	// TokensOnDeath is multiplied by this, so an elite is worth more for each mutation it has.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	float TokenMultiplier = 1.0f;

	// The mutations and the base type, such as "Swift Siphon Shade".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Elite")
	FText DisplayName;

	bool HasMutation(const ENightlightEliteMutation Mutation) const { return Mutations.Contains(Mutation); }
};

/*
References

Epic Games, Inc., 2026. Structs in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/structs-in-unreal-engine>
[Accessed 6 November 2026].
*/
