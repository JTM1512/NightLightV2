#pragma once

#include "CoreMinimal.h"
#include "NightlightEnemy.h"
#include "NightlightEnemyShade.generated.h"

class ANightlightEnemyProjectile;

// A fragile ranged enemy. It stops at long range to shoot defenders instead of walking up to them.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightEnemyShade : public ANightlightEnemy
{
	GENERATED_BODY()

public:
	ANightlightEnemyShade();

	// CoreAttackRange limited to stay inside the assigned Core's AttackRange. Public so tests and the HUD can read it.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy|Shade|Core Attack")
	float GetEffectiveCoreAttackRange() const;

protected:
	// Set this to BP_EnemyProjectile so the shot has a visible mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Shade")
	TSubclassOf<ANightlightEnemyProjectile> ProjectileClass;

	// Shots start above the Shade's origin so they do not clip into the path.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Shade")
	FVector ProjectileSpawnOffset = FVector(0.0f, 0.0f, 60.0f);

	// The Shade stops this far from the Core and shoots it. It is always kept inside the Core's own
	// AttackRange, so the Core can still destroy a Shade that reaches it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Shade|Core Attack", meta = (ClampMin = "0.0"))
	float CoreAttackRange = 700.0f;

	// How far inside the Core's AttackRange the Shade must stay when CoreAttackRange is set too high.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Shade|Core Attack", meta = (ClampMin = "0.0"))
	float CoreRangeSafetyMargin = 100.0f;

	// Each shot at the Core deals CoreDamage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Shade|Core Attack", meta = (ClampMin = "0.1"))
	float CoreAttackInterval = 2.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy|Shade|Core Attack")
	bool bIsAttackingCore = false;

	// Fires a projectile instead of damaging the defender directly.
	virtual void AttackDefender(ANightlightDefender* Defender) override;

	// Stops to attack the Core once it is inside CoreAttackRange.
	virtual void MoveAlongRoute(float DeltaTime) override;

	// A Shade that runs out of route attacks the Core from where it stands instead of walking into it.
	virtual void HandleCoreReached() override;

	virtual void Die() override;

	// Spawns a projectile facing the target. Returns null when no projectile could be spawned.
	ANightlightEnemyProjectile* FireProjectileAt(AActor* Target, float Damage);

private:
	FTimerHandle CoreAttackTimerHandle;
	bool bHasWarnedAboutCoreRange = false;

	void StartCoreAttack();
	void AttackCore();
};
