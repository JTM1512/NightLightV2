#include "NightlightDreamCore.h"
#include "../Enemies/NightlightEnemy.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

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

	// The Core attacks on a timer so it does not search for enemies every frame.
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

	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsOfClass(this, ANightlightEnemy::StaticClass(), FoundEnemies);

	ANightlightEnemy* ClosestEnemy = nullptr;
	float ClosestDistanceSquared = FMath::Square(AttackRange);

	for (AActor* FoundActor : FoundEnemies)
	{
		ANightlightEnemy* Enemy = Cast<ANightlightEnemy>(FoundActor);
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Enemy->GetActorLocation());
		if (DistanceSquared <= ClosestDistanceSquared)
		{
			ClosestEnemy = Enemy;
			ClosestDistanceSquared = DistanceSquared;
		}
	}

	if (ClosestEnemy)
	{
		ClosestEnemy->ApplyDamage(AttackDamage);
	}
}
