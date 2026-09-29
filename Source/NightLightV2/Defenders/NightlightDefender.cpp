#include "NightlightDefender.h"
#include "../Enemies/NightlightEnemy.h"
#include "../Systems/NightlightActorRegistrySubsystem.h"
#include "../UI/NightlightHealthWidgetUtils.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"

ANightlightDefender::ANightlightDefender()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANightlightDefender::BeginPlay()
{
	Super::BeginPlay();

	// The registry lets enemies find this defender without an actor search.
	if (UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>())
	{
		Registry->RegisterDefender(this);
	}

	// Blueprint defender types can change MaxHealth, so copy it when the game starts.
	MaxHealth = FMath::Max(MaxHealth, 0.0f);
	CurrentHealth = MaxHealth;
	bIsDead = false;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	NightlightHealthWidgetUtils::UpdateWorldHealthWidget(this, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		Die();
		return;
	}

	if (!bAutoAttack)
	{
		return;
	}

	// A looping timer attacks at a fixed rate without searching for enemies every frame
	// (Epic Games, Inc., 2026d).
	GetWorldTimerManager().SetTimer(
		AttackTimerHandle,
		this,
		&ANightlightDefender::HandleAttackTimer,
		FMath::Max(AttackInterval, 0.1f),
		true);
}

void ANightlightDefender::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EndPlay runs when the defender is destroyed or the level ends, so both remove it from the
	// registry (Epic Games, Inc., 2026g).
	if (UWorld* const World = GetWorld())
	{
		if (UNightlightActorRegistrySubsystem* const Registry = World->GetSubsystem<UNightlightActorRegistrySubsystem>())
		{
			Registry->UnregisterDefender(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

float ANightlightDefender::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* const EventInstigator,
	AActor* const DamageCauser)
{
	// Unreal's standard damage calls also reach the defender's own health
	// (Epic Games, Inc., 2026b).
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	ApplyDamage(AppliedDamage);
	return AppliedDamage;
}

void ANightlightDefender::ApplyDamage(const float DamageAmount)
{
	if (bIsDead || DamageAmount <= 0.0f)
	{
		return;
	}

	// A strong attack can reach zero, but health must never become negative.
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	OnDamageTaken.Broadcast(AppliedDamage);
	NightlightHealthWidgetUtils::UpdateWorldHealthWidget(this, CurrentHealth, MaxHealth, AppliedDamage);

	if (CurrentHealth > 0.0f)
	{
		return;
	}

	Die();
}

ANightlightEnemy* ANightlightDefender::FindTarget_Implementation()
{
	// The default target is the closest living enemy inside the attack range, found the same way as
	// the Dream Core finds its targets (Epic Games, Inc., 2026f).
	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsOfClass(this, ANightlightEnemy::StaticClass(), FoundEnemies);

	ANightlightEnemy* ClosestEnemy = nullptr;
	float ClosestDistanceSquared = FMath::Square(FMath::Max(AttackRange, 0.0f));

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

	return ClosestEnemy;
}

void ANightlightDefender::PerformAttack_Implementation(ANightlightEnemy* const Target)
{
	if (IsValid(Target))
	{
		Target->ApplyDamage(FMath::Max(AttackDamage, 0.0f));
	}
}

void ANightlightDefender::HandleAttackTimer()
{
	if (bIsDead || AttackRange <= 0.0f || AttackDamage <= 0.0f)
	{
		return;
	}

	ANightlightEnemy* const Target = FindTarget();
	if (IsValid(Target) && !Target->IsDead())
	{
		PerformAttack(Target);
	}
}

void ANightlightDefender::Die()
{
	if (bIsDead)
	{
		return;
	}

	// Set this first so a listener cannot kill the defender twice.
	bIsDead = true;
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	OnDefenderDied.Broadcast();

	// The placement platform only frees up when the defender is destroyed
	// (Epic Games, Inc., 2026a).
	Destroy();
}

/*
References

Epic Games, Inc., 2026a. AActor::Destroy. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AActor/Destroy>
[Accessed 29 September 2026].

Epic Games, Inc., 2026b. AActor::TakeDamage. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AActor/TakeDamage>
[Accessed 29 September 2026].

Epic Games, Inc., 2026c. Dynamic Delegates in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/dynamic-delegates-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026d. Gameplay Timers in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-timers-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026e. UFunctions in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/ufunctions-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026f. UGameplayStatics::GetAllActorsOfClass. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameplayStatics/GetAllActorsOfClass>
[Accessed 29 September 2026].

Epic Games, Inc., 2026g. Unreal Engine Actor Lifecycle. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-actor-lifecycle>
[Accessed 29 September 2026].
*/
