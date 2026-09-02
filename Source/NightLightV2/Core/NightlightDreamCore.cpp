#include "NightlightDreamCore.h"
#include "Components/SceneComponent.h"

ANightlightDreamCore::ANightlightDreamCore()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
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
	}
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
		OnCoreDestroyed.Broadcast();
	}
}
