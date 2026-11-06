#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "../Systems/NightlightUpgradeTypes.h"
#include "NightlightDreamCore.generated.h"

class ULightComponent;
class UNiagaraSystem;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNightlightCoreHealthChangedSignature,
	float, CurrentHealth,
	float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNightlightCoreDestroyedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNightlightCoreUpgradedSignature,
	int32, NewUpgradeLevel,
	int32, MaxUpgradeLevel);

// Keeps the Core health separate from generation and any later win or loss UI.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightDreamCore : public AActor
{
	GENERATED_BODY()

public:
	ANightlightDreamCore();

	// Enemies call this after finishing their route. Further damage is ignored once the Core reaches zero.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Dream Core")
	void ApplyCoreDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core")
	bool IsCoreDestroyed() const { return bCoreDestroyed; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core")
	float GetMaxHealth() const { return MaxHealth; }

	// Current health over max health, from 0 to 1. The crayon HUD uses it as the kid's sanity.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core")
	float GetHealthPercent() const;

	// Ranged enemies read this so they always stop close enough for the Core to hit them.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core|Attack")
	float GetAttackRange() const { return AttackRange; }

	// The HUD checks CanAfford with GetNextUpgradeCost, spends the tokens, then calls this. Returns false,
	// and changes nothing, when the Core is destroyed or already fully upgraded.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Dream Core|Upgrades")
	bool UpgradeCore();

	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core|Upgrades")
	bool CanUpgradeCore() const;

	// 0 before any upgrade is bought.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core|Upgrades")
	int32 GetUpgradeLevel() const { return UpgradeLevel; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core|Upgrades")
	int32 GetMaxUpgradeLevel() const { return GetUpgradeTiers().Num(); }

	// Returns false when there is no next tier, so the HUD can hide or disable its button.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core|Upgrades")
	bool GetNextUpgradeTier(FNightlightUpgradeTier& OutTier) const;

	// -1 when the Core is fully upgraded.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core|Upgrades")
	int32 GetNextUpgradeCost() const;

	// The starting limit plus the slots of every tier bought so far.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core|Defenders")
	int32 GetMaxDefenders() const;

	// Living defenders only, so a slot frees up when a defender dies or is sold.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core|Defenders")
	int32 GetDefenderCount() const;

	// The player controller checks this before it places a defender.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Dream Core|Defenders")
	bool CanPlaceDefender() const { return GetDefenderCount() < GetMaxDefenders(); }

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Dream Core")
	FNightlightCoreHealthChangedSignature OnCoreHealthChanged;

	// The HUD binds this to refresh its upgrade button and defender limit.
	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Dream Core|Upgrades")
	FNightlightCoreUpgradedSignature OnCoreUpgraded;

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Dream Core")
	FNightlightCoreDestroyedSignature OnCoreDestroyed;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// Assign the Core's visual mesh in BP_DreamCore.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CoreMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Dream Core", meta = (ClampMin = "0.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Dream Core")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Dream Core|Attack", meta = (ClampMin = "0.0"))
	float AttackRange = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Dream Core|Attack", meta = (ClampMin = "0.0"))
	float AttackDamage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Dream Core|Attack", meta = (ClampMin = "0.1"))
	float AttackInterval = 1.0f;

	// How many defenders the player may have in play before any upgrade.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Dream Core|Defenders", meta = (ClampMin = "0"))
	int32 BaseMaxDefenders = 6;

	// Optional. When set, its tiers replace the Upgrade Tiers below, so the Core and defenders can be
	// tuned in Data Assets of the same type.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Dream Core|Upgrades")
	TObjectPtr<UNightlightUpgradeData> UpgradeData;

	// Used when no Upgrade Data is set. Two tiers by default.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Dream Core|Upgrades")
	TArray<FNightlightUpgradeTier> UpgradeTiers;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Dream Core|Upgrades")
	int32 UpgradeLevel = 0;

	// Optional. Plays at the Core each time it is upgraded, in the new tier's colour and stronger at
	// each level.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Dream Core|Upgrades")
	TObjectPtr<UNiagaraSystem> UpgradeBurstSystem;

	// Size of the burst's ground ring, in centimetres.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Dream Core|Upgrades", meta = (ClampMin = "0.0"))
	float UpgradeBurstRadius = 300.0f;

private:
	bool bCoreDestroyed = false;
	FTimerHandle AttackTimerHandle;

	// Saved at BeginPlay so each tier scales and tints from the Blueprint's own look.
	FVector BaseMeshScale = FVector::OneVector;
	float BaseLightIntensity = 0.0f;
	bool bVisualBaseSaved = false;

	const TArray<FNightlightUpgradeTier>& GetUpgradeTiers() const;
	ULightComponent* FindCoreLight() const;
	void SaveVisualBase();
	void ApplyTierVisuals(const FNightlightUpgradeTier& Tier);

	// One attack damages the closest living enemy inside the Core's range.
	void AttackNearestEnemy();
};
