#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "NightlightDreamCore.generated.h"

class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNightlightCoreHealthChangedSignature,
	float, CurrentHealth,
	float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNightlightCoreDestroyedSignature);

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

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Dream Core")
	FNightlightCoreHealthChangedSignature OnCoreHealthChanged;

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

private:
	bool bCoreDestroyed = false;
	FTimerHandle AttackTimerHandle;

	// One attack damages the closest living enemy inside the Core's range.
	void AttackNearestEnemy();
};
