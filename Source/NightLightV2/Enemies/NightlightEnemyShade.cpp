#include "NightlightEnemyShade.h"
#include "NightlightEnemyProjectile.h"
#include "../Core/NightlightDreamCore.h"
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

void ANightlightEnemyShade::MoveAlongRoute(const float DeltaTime)
{
	const ANightlightDreamCore* const Core = GetDreamCore();
	if (!bIsDead && !bIsAttackingCore && IsValid(Core) && !Core->IsCoreDestroyed())
	{
		const float EffectiveRange = GetEffectiveCoreAttackRange();
		if (!bHasWarnedAboutCoreRange && EffectiveRange < CoreAttackRange)
		{
			// Balance rule: a Shade outside the Core's range could never be killed by it, so the range
			// is limited and the designer is told once (Epic Games, Inc., 2026b).
			bHasWarnedAboutCoreRange = true;
			UE_LOG(LogTemp, Warning,
				TEXT("%s CoreAttackRange %.0f is not inside the Dream Core's AttackRange %.0f, so %.0f is used instead."),
				*GetName(), CoreAttackRange, Core->GetAttackRange(), EffectiveRange);
		}

		if (FVector::DistSquared(GetActorLocation(), Core->GetActorLocation()) <= FMath::Square(EffectiveRange))
		{
			StartCoreAttack();
			return;
		}
	}

	Super::MoveAlongRoute(DeltaTime);
}

void ANightlightEnemyShade::HandleCoreReached()
{
	StartCoreAttack();
}

void ANightlightEnemyShade::Die()
{
	GetWorldTimerManager().ClearTimer(CoreAttackTimerHandle);
	Super::Die();
}

float ANightlightEnemyShade::GetEffectiveCoreAttackRange() const
{
	const float DesiredRange = FMath::Max(CoreAttackRange, 0.0f);
	const ANightlightDreamCore* const Core = GetDreamCore();
	if (!IsValid(Core))
	{
		return DesiredRange;
	}

	// Stay a margin inside the Core's range. A very small Core range falls back to half of it.
	const float CoreRange = FMath::Max(Core->GetAttackRange(), 0.0f);
	const float MaxAllowedRange = FMath::Max(CoreRange - FMath::Max(CoreRangeSafetyMargin, 0.0f), CoreRange * 0.5f);
	return FMath::Min(DesiredRange, MaxAllowedRange);
}

void ANightlightEnemyShade::StartCoreAttack()
{
	if (bIsDead || bIsAttackingCore)
	{
		return;
	}

	// bHasReachedCore stops the base class from moving or fighting defenders, but the Shade stays alive
	// and is not removed like a walker.
	bIsAttackingCore = true;
	bHasReachedCore = true;
	SetActorTickEnabled(false);
	ClearDefenderTarget();

	// The first shot fires straight away, then the timer repeats it until the Shade dies
	// (Epic Games, Inc., 2026a).
	AttackCore();
	if (!bIsDead)
	{
		GetWorldTimerManager().SetTimer(
			CoreAttackTimerHandle,
			this,
			&ANightlightEnemyShade::AttackCore,
			FMath::Max(CoreAttackInterval, 0.1f),
			true);
	}
}

void ANightlightEnemyShade::AttackCore()
{
	ANightlightDreamCore* const Core = GetDreamCore();
	if (bIsDead || !IsValid(Core) || Core->IsCoreDestroyed())
	{
		GetWorldTimerManager().ClearTimer(CoreAttackTimerHandle);
		return;
	}

	FireProjectileAt(Core, FMath::Max(CoreDamage, 0.0f));
}

ANightlightEnemyProjectile* ANightlightEnemyShade::FireProjectileAt(AActor* const Target, const float Damage)
{
	if (!IsValid(Target) || !ProjectileClass)
	{
		return nullptr;
	}

	// The projectile moves along its forward axis, so it is spawned already facing the target.
	// AlwaysSpawn stops the shot from being skipped when it starts inside the Shade's own mesh
	// (Epic Games, Inc., 2026c).
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

Epic Games, Inc., 2026a. Gameplay Timers in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-timers-in-unreal-engine>
[Accessed 30 September 2026].

Epic Games, Inc., 2026b. Logging in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/logging-in-unreal-engine>
[Accessed 30 September 2026].

Epic Games, Inc., 2026c. UWorld::SpawnActor. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorld/SpawnActor>
[Accessed 30 September 2026].
*/
