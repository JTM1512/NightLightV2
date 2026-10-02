#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "NightlightEnemy.generated.h"

class USceneComponent;
class UMaterialInstanceDynamic;
class ANightlightDreamCore;
class ANightlightDefender;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNightlightEnemyHealthChangedSignature,
	float, CurrentHealth,
	float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNightlightEnemyDiedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNightlightEnemyDamagedSignature,
	float, DamageAmount);

// An enemy follows one generated route from its Rift to the Dream Core.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightEnemy : public AActor
{
	GENERATED_BODY()

public:
	ANightlightEnemy();

	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	// Copies the route, places the enemy at the Rift and starts its movement.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Enemy")
	void AssignRoute(const TArray<FVector>& RoutePoints);

	// Set by the spawner so every route damages the same Core.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Enemy")
	void AssignDreamCore(ANightlightDreamCore* InDreamCore);

	// Defenders only call this function. Their targeting and attack code stays in the defender.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Enemy")
	void ApplyDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy")
	bool IsDead() const { return bIsDead; }

	// The wave director counts an enemy as stopped when it leaves play dead without having reached the Core.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy")
	bool HasReachedCore() const { return bHasReachedCore; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy")
	float GetMaxHealth() const { return MaxHealth; }

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Enemy")
	FNightlightEnemyHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Enemy")
	FNightlightEnemyDiedSignature OnEnemyDied;

	// The Blueprint health bar uses this to show the amount taken by the latest hit.
	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Enemy")
	FNightlightEnemyDamagedSignature OnDamageTaken;

	// The enemy triggers the reward, while its Blueprint passes the tokens to the existing Game State pool.
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Nightlight|Enemy|Rewards")
	void AwardDeathTokens(int32 TokenAmount);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Blueprint children can attach their mesh and other visuals to this root.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float MovementSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float WaypointAcceptanceDistance = 10.0f;

	// How quickly the enemy turns to keep facing the Dream Core. Zero snaps straight to it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float TurnSpeed = 8.0f;

	// The enemy floats this far above the terrain and stays level. Zero walks on the slope instead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float HoverHeight = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float CoreDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Defender Attack", meta = (ClampMin = "0.0"))
	float DefenderAttackRange = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Defender Attack", meta = (ClampMin = "0.0"))
	float DefenderAttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Defender Attack", meta = (ClampMin = "0.1"))
	float DefenderAttackInterval = 1.0f;

	// Each enemy controls its own reward while the Game State remains responsible for the player's token pool.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Enemy|Rewards", meta = (ClampMin = "0"))
	int32 TokensOnDeath = 10;

	// The route is copied so the enemy does not need to keep checking the generator.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy")
	TArray<FVector> AssignedRoutePoints;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy")
	int32 CurrentWaypointIndex = INDEX_NONE;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy")
	bool bHasReachedCore = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy")
	bool bIsDead = false;

	// Every mesh material with a scalar parameter of this name flashes when the enemy is hit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Feedback")
	FName HitFlashParameterName = TEXT("HitFlash");

	// How long the flash takes to fade from full back to nothing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Feedback", meta = (ClampMin = "0.0"))
	float HitFlashDuration = 0.15f;

	// A dead enemy stays in the level this long, without collision, so its death effect can play. Zero removes it at once.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Feedback", meta = (ClampMin = "0.0"))
	float DeathRemovalDelay = 0.2f;

	// Lets the enemy Blueprint react just before it is removed at the Core.
	UFUNCTION(BlueprintImplementableEvent, Category = "Nightlight|Enemy", meta = (DisplayName = "On Core Reached"))
	void OnCoreReached();

	// Lets the enemy Blueprint play an animation or effect on each hit against a defender.
	UFUNCTION(BlueprintImplementableEvent, Category = "Nightlight|Enemy|Defender Attack", meta = (DisplayName = "On Attack Defender"))
	void OnAttackDefender(ANightlightDefender* Defender);

	// Lets the enemy Blueprint spawn a burst or play a sound when it dies. It runs before the enemy is removed.
	UFUNCTION(BlueprintImplementableEvent, Category = "Nightlight|Enemy|Feedback", meta = (DisplayName = "On Death Effects"))
	void OnDeathEffects();

	// New enemy types override only the steps that differ. The search delay and attack timer stay in this class.
	virtual void MoveAlongRoute(float DeltaTime);

	// Returns the closest living defender in range, or null when there is none.
	virtual ANightlightDefender* FindDefenderTarget();

	// A single hit. It runs as soon as a target is found, then on every attack interval.
	virtual void AttackDefender(ANightlightDefender* Defender);

	// Walkers stop to fight. A type that returns false keeps moving while it attacks.
	virtual bool ShouldStopForDefender() const;

	// Damages the Core once, then removes the enemy.
	virtual void HandleCoreReached();

	virtual void Die();

	// Ranged types fire their projectiles at the same Core the spawner assigned.
	ANightlightDreamCore* GetDreamCore() const { return DreamCore; }

	// Stops the defender attack timer, for example when a type switches to attacking the Core.
	void ClearDefenderTarget();

private:
	UPROPERTY()
	TObjectPtr<ANightlightDreamCore> DreamCore;

	UPROPERTY()
	TObjectPtr<ANightlightDefender> TargetDefender;

	FTimerHandle DefenderAttackTimerHandle;
	float TimeUntilDefenderSearch = 0.0f;

	// The middle of the Core's meshes, because the tower mesh's pivot sits on one corner.
	FVector CoreFacingLocation = FVector::ZeroVector;

	// Created on the first hit, so enemies that are never hit keep sharing their original materials.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> HitFlashMaterials;

	bool bHasCreatedHitFlashMaterials = false;
	FTimerHandle HitFlashTimerHandle;
	float HitFlashEndTime = 0.0f;

	void CreateHitFlashMaterials();
	void StartHitFlash();
	void UpdateHitFlash();
	void SetHitFlashAmount(float Amount);

	bool UpdateDefenderCombat(float DeltaTime);
	void AttackTargetDefender();
	void ReachNextWaypoint();
	void FaceCoreOnTerrain(float DeltaTime);
};
