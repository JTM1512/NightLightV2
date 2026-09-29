#include "NightlightDreamCore.h"
#include "../Enemies/NightlightEnemy.h"
#include "../Systems/NightlightActorRegistrySubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ANightlightDreamCore::ANightlightDreamCore()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(SceneRoot);
}

void ANightlightDreamCore::BeginPlay()
{
	Super::BeginPlay();

	// Blueprint children can change MaxHealth, so copy it when the game starts.
	MaxHealth = FMath::Max(MaxHealth, 0.0f);
	CurrentHealth = MaxHealth;
	bCoreDestroyed = false;
	OnCoreHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		bCoreDestroyed = true;
		OnCoreDestroyed.Broadcast();
		return;
	}

	// The Core attacks on a timer so it does not search for enemies every frame
	// (Epic Games, Inc., 2026a).
	GetWorldTimerManager().SetTimer(
		AttackTimerHandle,
		this,
		&ANightlightDreamCore::AttackNearestEnemy,
		FMath::Max(AttackInterval, 0.1f),
		true);
}

void ANightlightDreamCore::ApplyCoreDamage(const float DamageAmount)
{
	if (bCoreDestroyed || DamageAmount <= 0.0f)
	{
		return;
	}

	// A large hit can reach zero, but it must never leave the Core with negative health.
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	OnCoreHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		// Set this first because an event listener could try to damage the Core again.
		bCoreDestroyed = true;
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		OnCoreDestroyed.Broadcast();
	}
}

void ANightlightDreamCore::AttackNearestEnemy()
{
	if (bCoreDestroyed || AttackRange <= 0.0f || AttackDamage <= 0.0f)
	{
		return;
	}

	// Every enemy registers itself, so the registry replaces a GetAllActorsOfClass search, which is
	// slow when there are many actors (Epic Games, Inc., 2026b). Dead enemies are skipped.
	const UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>();
	ANightlightEnemy* const ClosestEnemy = Registry ? Registry->FindClosestEnemy(GetActorLocation(), AttackRange) : nullptr;
	if (ClosestEnemy)
	{
		ClosestEnemy->ApplyDamage(AttackDamage);
	}
}

/*
References

Epic Games, Inc., 2026a. Gameplay Timers in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-timers-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026b. UGameplayStatics::GetAllActorsOfClass. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameplayStatics/GetAllActorsOfClass>
[Accessed 29 September 2026].
*/
