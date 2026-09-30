#include "NightlightEnemyShade.h"
#include "NightlightEnemyProjectile.h"
#include "../Defenders/NightlightDefender.h"
#include "Engine/World.h"

ANightlightEnemyShade::ANightlightEnemyShade()
{
	// Starting values for a fast, fragile ranged enemy. Tune them in BP_EnemyShade.
	MovementSpeed = 360.0f;
	MaxHealth = 60.0f;
	CurrentHealth = 60.0f;
	DefenderAttackRange = 900.0f;
	DefenderAttackDamage = 8.0f;
	DefenderAttackInterval = 1.5f;
	TokensOnDeath = 20;

	// The plain C++ projectile still works if the Blueprint forgets to set one, it just has no mesh.
	ProjectileClass = ANightlightEnemyProjectile::StaticClass();
}

void ANightlightEnemyShade::AttackDefender(ANightlightDefender* const Defender)
{
	if (IsValid(Defender))
	{
		FireProjectileAt(Defender, FMath::Max(DefenderAttackDamage, 0.0f));
	}
}

ANightlightEnemyProjectile* ANightlightEnemyShade::FireProjectileAt(AActor* const Target, const float Damage)
{
	if (!IsValid(Target) || !ProjectileClass)
	{
		return nullptr;
	}

	// The projectile moves along its forward axis, so it is spawned already facing the target.
	// AlwaysSpawn stops the shot from being skipped when it starts inside the Shade's own mesh
	// (Epic Games, Inc., 2026).
	const FVector SpawnLocation = GetActorLocation() + ProjectileSpawnOffset;
	const FRotator SpawnRotation = (Target->GetActorLocation() - SpawnLocation).Rotation();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANightlightEnemyProjectile* const Projectile = GetWorld()->SpawnActor<ANightlightEnemyProjectile>(
		ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParameters);

	if (Projectile)
	{
		Projectile->Launch(Target, Damage);
	}

	return Projectile;
}

/*
References

Epic Games, Inc., 2026. UWorld::SpawnActor. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorld/SpawnActor>
[Accessed 30 September 2026].
*/
