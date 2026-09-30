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

protected:
	// Set this to BP_EnemyProjectile so the shot has a visible mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Shade")
	TSubclassOf<ANightlightEnemyProjectile> ProjectileClass;

	// Shots start above the Shade's origin so they do not clip into the path.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Shade")
	FVector ProjectileSpawnOffset = FVector(0.0f, 0.0f, 60.0f);

	// Fires a projectile instead of damaging the defender directly.
	virtual void AttackDefender(ANightlightDefender* Defender) override;

	// Spawns a projectile facing the target. Returns null when no projectile could be spawned.
	ANightlightEnemyProjectile* FireProjectileAt(AActor* Target, float Damage);
};
