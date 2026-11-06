#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "NightlightDefender.generated.h"

class USceneComponent;
class UNiagaraSystem;
class ANightlightEnemy;

// Dynamic multicast delegates let several Blueprints, such as the health bar, bind to the same
// event (Epic Games, Inc., 2026c).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNightlightDefenderHealthChangedSignature,
	float, CurrentHealth,
	float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNightlightDefenderDiedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNightlightDefenderDamagedSignature,
	float, DamageAmount);

// How a defender fights, so the wave director can pick a wave template that counters the player's build.
UENUM(BlueprintType)
enum class ENightlightDefenderStyle : uint8
{
	Standard,
	ShortRange,
	LongRange
};

// Every defender Blueprint is parented to this class, so enemies can damage any defender type the same way.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightDefender : public AActor
{
	GENERATED_BODY()

public:
	ANightlightDefender();

	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	// Enemies only call this function. The defender is destroyed at zero so its platform can be used again.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Defender")
	void ApplyDamage(float DamageAmount);

	// Remnants call this to restore health. Health never goes above MaxHealth.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Defender")
	void Heal(float HealAmount);

	UFUNCTION(BlueprintPure, Category = "Nightlight|Defender")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Defender")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Defender")
	float GetMaxHealth() const { return MaxHealth; }

	// Current health over max health, from 0 to 1.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Defender")
	float GetHealthPercent() const;

	// Blueprints use these instead of writing the variables directly, so the value is clamped, the
	// health changed event fires and the world health bar stays in sync. Setting health to zero kills the defender.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Defender")
	void SetCurrentHealth(float NewHealth);

	// Current health is clamped to the new max, so lowering the max can also lower the current health.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Defender")
	void SetMaxHealth(float NewMaxHealth, bool bHealToFull = false);

	// The wave director reads these to measure how well each spawn route is defended. BlueprintPure, so they
	// show as getter nodes without an execution pin (Epic Games, Inc., 2026f).
	UFUNCTION(BlueprintPure, Category = "Nightlight|Defender|Attack")
	float GetAttackRange() const { return AttackRange; }

	// Attack Damage divided by Attack Interval, with the same minimum interval the attack timer uses.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Defender|Attack")
	float GetDamagePerSecond() const;

	UFUNCTION(BlueprintPure, Category = "Nightlight|Defender")
	ENightlightDefenderStyle GetDefenderStyle() const { return DefenderStyle; }

	// The upgrade itself lives in Blueprint, so the HUD calls this once an upgrade has raised the level.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Defender|Upgrades")
	void PlayUpgradeBurst(int32 NewUpgradeLevel);

	// Heals to the current max and refreshes the health bar. The HUD calls this after an upgrade raised the
	// level, so the defender comes back at full health with its new max.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Defender|Upgrades")
	void RestoreFullHealth();

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Defender")
	FNightlightDefenderHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Defender")
	FNightlightDefenderDiedSignature OnDefenderDied;

	// The Blueprint health bar uses this to show the amount taken by the latest hit.
	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Defender")
	FNightlightDefenderDamagedSignature OnDamageTaken;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Blueprint children can attach their mesh and other visuals to this root.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// Same names as the old BP_Defender variables so the Blueprint can be reparented without renaming nodes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender", meta = (ClampMin = "0.0"))
	float MaxHealth = 100.0f;

	// Each defender Blueprint sets its own style in its class defaults.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nightlight|Defender")
	ENightlightDefenderStyle DefenderStyle = ENightlightDefenderStyle::Standard;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Defender")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Defender")
	bool bIsDead = false;

	// Off by default because BP_Defender already has its own Blueprint attack. Turning this on
	// for it after the reparent would make it attack twice. New defender types turn it on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender|Attack")
	bool bAutoAttack = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender|Attack", meta = (ClampMin = "0.0"))
	float AttackRange = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender|Attack", meta = (ClampMin = "0.0"))
	float AttackDamage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender|Attack", meta = (ClampMin = "0.1"))
	float AttackInterval = 1.0f;

	// How quickly the defender turns to face its current target. Zero snaps straight to it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender|Attack", meta = (ClampMin = "0.0"))
	float TurnSpeed = 8.0f;

	// A moving defender, like the patroller, flies this far above the terrain. Zero drives on the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender", meta = (ClampMin = "0.0"))
	float HoverHeight = 0.0f;

	// Optional. Plays at the defender each time it is upgraded, stronger at each level.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender|Upgrades")
	TObjectPtr<UNiagaraSystem> UpgradeBurstSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender|Upgrades")
	FLinearColor UpgradeBurstColor = FLinearColor(1.0f, 0.8f, 0.35f);

	// Size of the burst's ground ring, in centimetres.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender|Upgrades", meta = (ClampMin = "0.0"))
	float UpgradeBurstRadius = 150.0f;

	// New defender types only override FindTarget and PerformAttack, in C++ or Blueprint.
	// BlueprintNativeEvent keeps a C++ default that a Blueprint can replace (Epic Games, Inc., 2026f).
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Nightlight|Defender|Attack")
	ANightlightEnemy* FindTarget();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Nightlight|Defender|Attack")
	void PerformAttack(ANightlightEnemy* Target);

private:
	FTimerHandle AttackTimerHandle;

	UPROPERTY()
	TObjectPtr<ANightlightEnemy> FacingTarget;

	float TimeUntilTargetSearch = 0.0f;

	// Kept separately so a Blueprint that also sets the rotation, like the patroller, cannot pull
	// the defender away from its target.
	float FacingYaw = 0.0f;
	FVector LastLocation = FVector::ZeroVector;

	void HandleAttackTimer();
	void Die();
};
