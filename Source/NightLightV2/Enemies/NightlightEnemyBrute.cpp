#include "NightlightEnemyBrute.h"
#include "../Defenders/NightlightDefender.h"
#include "../Systems/NightlightActorRegistrySubsystem.h"
#include "Engine/World.h"

ANightlightEnemyBrute::ANightlightEnemyBrute()
{
	// Starting values for a slow tank: half the walker's speed, four times its health and three times
	// its Core damage. Tune them in BP_EnemyBrute.
	MovementSpeed = 150.0f;
	MaxHealth = 400.0f;
	CurrentHealth = 400.0f;
	CoreDamage = 30.0f;

	// A short reach, so the Brute has to walk up to defenders before it can slam them. The interval is
	// longer than the wind-up so one slam always lands before the next one starts.
	DefenderAttackRange = 300.0f;
	DefenderAttackDamage = 20.0f;
	DefenderAttackInterval = 2.0f;

	// The hardest enemy to kill is worth the most tokens.
	TokensOnDeath = 40;
}

void ANightlightEnemyBrute::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Reaching the Core or the level ending removes the Brute without Die, so the wind-up is also
	// cleared here (Epic Games, Inc., 2026c).
	CancelSlam();
	Super::EndPlay(EndPlayReason);
}

void ANightlightEnemyBrute::AttackDefender(ANightlightDefender* const Defender)
{
	if (bIsDead || bIsWindingUpSlam || !IsValid(Defender))
	{
		return;
	}

	// The slam lands after a one-shot timer, so the wind-up can be read and cancelled if the Brute dies
	// first (Epic Games, Inc., 2026a).
	bIsWindingUpSlam = true;
	OnSlamWindUp();
	if (SlamWindUpTime <= 0.0f)
	{
		LandSlam();
		return;
	}

	GetWorldTimerManager().SetTimer(
		SlamWindUpTimerHandle,
		this,
		&ANightlightEnemyBrute::LandSlam,
		SlamWindUpTime,
		false);
}

void ANightlightEnemyBrute::MoveAlongRoute(const float DeltaTime)
{
	if (bIsWindingUpSlam)
	{
		return;
	}

	Super::MoveAlongRoute(DeltaTime);
}

void ANightlightEnemyBrute::Die()
{
	// A Brute killed mid wind-up must never land its slam.
	CancelSlam();
	Super::Die();
}

void ANightlightEnemyBrute::LandSlam()
{
	GetWorldTimerManager().ClearTimer(SlamWindUpTimerHandle);
	bIsWindingUpSlam = false;
	if (bIsDead || bHasReachedCore)
	{
		return;
	}

	// The registry already holds every living defender, so the slam checks those instead of running a
	// physics overlap. Copying the list first keeps it safe when a hit destroys a defender.
	const UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}

	const TArray<ANightlightDefender*> Defenders = Registry->GetDefenders();
	const FVector SlamCentre = GetActorLocation();
	const float SlamRadiusSquared = FMath::Square(FMath::Max(SlamRadius, 0.0f));
	const float SlamDamage = FMath::Max(DefenderAttackDamage, 0.0f);

	int32 HitCount = 0;
	for (ANightlightDefender* const Defender : Defenders)
	{
		if (IsValid(Defender) && !Defender->IsDead()
			&& FVector::DistSquared(SlamCentre, Defender->GetActorLocation()) <= SlamRadiusSquared)
		{
			Defender->ApplyDamage(SlamDamage);
			++HitCount;
		}
	}

	// Blueprints implement this without any C++ body (Epic Games, Inc., 2026b).
	OnSlamLanded(HitCount);
}

void ANightlightEnemyBrute::CancelSlam()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlamWindUpTimerHandle);
	}

	bIsWindingUpSlam = false;
}

/*
References

Epic Games, Inc., 2026a. Gameplay Timers in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-timers-in-unreal-engine>
[Accessed 30 September 2026].

Epic Games, Inc., 2026b. UFunctions in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/ufunctions-in-unreal-engine>
[Accessed 30 September 2026].

Epic Games, Inc., 2026c. Unreal Engine Actor Lifecycle. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-actor-lifecycle>
[Accessed 30 September 2026].
*/
