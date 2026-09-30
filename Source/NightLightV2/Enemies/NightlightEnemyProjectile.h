#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightlightEnemyProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;
struct FHitResult;

// A shot fired by a ranged enemy. It damages the defender or Dream Core it reaches, then removes itself.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightEnemyProjectile : public AActor
{
	GENERATED_BODY()

public:
	ANightlightEnemyProjectile();

	virtual void Tick(float DeltaTime) override;

	// Called by the enemy straight after spawning. The target must be a defender or the Dream Core.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Enemy Projectile")
	void Launch(AActor* InTarget, float InDamage);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	// Assign a glowing mesh in the projectile Blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// Counts as a hit once the shot is this close to its target, even if the target's mesh has no overlap events.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy Projectile", meta = (ClampMin = "0.0"))
	float HitRadius = 60.0f;

	// Removes shots that miss, for example when the target was destroyed while they were in the air.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy Projectile", meta = (ClampMin = "0.1"))
	float Lifetime = 4.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy Projectile")
	float Damage = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy Projectile")
	TObjectPtr<AActor> Target;

	// Lets the projectile Blueprint play an impact effect before the shot is removed.
	UFUNCTION(BlueprintImplementableEvent, Category = "Nightlight|Enemy Projectile", meta = (DisplayName = "On Projectile Impact"))
	void OnProjectileImpact(AActor* HitActor);

private:
	bool bHasHit = false;

	UFUNCTION()
	void HandleOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	// Damages the actor if it is a living defender or the Dream Core. Returns false for anything else.
	bool TryHit(AActor* HitActor);
};
