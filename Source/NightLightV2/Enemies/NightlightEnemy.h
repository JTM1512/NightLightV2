#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "NightlightEnemy.generated.h"

class USceneComponent;
class ANightlightDreamCore;

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

	// Blueprint children can attach their mesh and other visuals to this root.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float MovementSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float WaypointAcceptanceDistance = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float CoreDamage = 10.0f;

	// Assign BP_Defender so the enemy only reacts to the teammate's defender type.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Defender Attack")
	TSubclassOf<AActor> DefenderClass;

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

	// Lets the enemy Blueprint react just before it is removed at the Core.
	UFUNCTION(BlueprintImplementableEvent, Category = "Nightlight|Enemy", meta = (DisplayName = "On Core Reached"))
	void OnCoreReached();

private:
	UPROPERTY()
	TObjectPtr<ANightlightDreamCore> DreamCore;

	UPROPERTY()
	TObjectPtr<AActor> TargetDefender;

	FTimerHandle DefenderAttackTimerHandle;
	float TimeUntilDefenderSearch = 0.0f;

	void MoveAlongRoute(float DeltaTime);
	bool UpdateDefenderCombat(float DeltaTime);
	void FindDefenderTarget();
	void AttackTargetDefender();
	void ClearDefenderTarget();
	void ReachNextWaypoint();
	void HandleCoreReached();
	void Die();
};
