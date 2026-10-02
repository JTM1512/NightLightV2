#include "NightlightDefender.h"
#include "../Enemies/NightlightEnemy.h"
#include "../Systems/NightlightActorRegistrySubsystem.h"
#include "../Systems/NightlightTerrainUtils.h"
#include "../UI/NightlightHealthWidgetUtils.h"
#include "Components/SceneComponent.h"

ANightlightDefender::ANightlightDefender()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANightlightDefender::BeginPlay()
{
	// Stand on top of whatever is under the spawn point, such as the placement crate, instead of
	// inside it. This runs before Super::BeginPlay so Blueprint children start from the raised
	// position (Epic Games, Inc., 2026i).
	FHitResult Hit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NightlightDefenderGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(
		Hit,
		GetActorLocation() + FVector(0.0, 0.0, 300.0),
		GetActorLocation() - FVector(0.0, 0.0, 50.0),
		ECC_Visibility,
		QueryParams))
	{
		SetActorLocation(Hit.ImpactPoint);
	}

	LastLocation = GetActorLocation();
	FacingYaw = GetActorRotation().Yaw;
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
	// (Epic Games, Inc., 2026e).
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
	// registry (Epic Games, Inc., 2026h).
	if (UWorld* const World = GetWorld())
	{
		if (UNightlightActorRegistrySubsystem* const Registry = World->GetSubsystem<UNightlightActorRegistrySubsystem>())
		{
			Registry->UnregisterDefender(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ANightlightDefender::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bIsDead)
	{
		return;
	}

	// Like the enemies' defender search, the target is refreshed on a short delay instead of every frame.
	TimeUntilTargetSearch -= DeltaTime;
	if (TimeUntilTargetSearch <= 0.0f)
	{
		TimeUntilTargetSearch = 0.2f;
		FacingTarget = FindTarget();
	}

	// An idle defender keeps the yaw its Blueprint gives it, such as the patroller's circling
	// (Epic Games, Inc., 2026d).
	if (IsValid(FacingTarget) && !FacingTarget->IsDead())
	{
		const FRotator Facing(0.0f, (FacingTarget->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.0f);
		FacingYaw = FMath::RInterpTo(FRotator(0.0f, FacingYaw, 0.0f), Facing, DeltaTime, TurnSpeed).Yaw;
	}
	else
	{
		FacingYaw = GetActorRotation().Yaw;
	}

	// Only a defender that moves, like the patroller, follows the terrain. The others stay on their crate.
	if (FVector2D(GetActorLocation() - LastLocation).IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, FacingYaw, 0.0f));
	}
	else
	{
		NightlightTerrainUtils::PlaceOnTerrain(this, FacingYaw, HoverHeight);
	}

	LastLocation = GetActorLocation();
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

float ANightlightDefender::GetDamagePerSecond() const
{
	// The attack timer never runs faster than every 0.1 seconds, so the same floor is used here.
	return FMath::Max(AttackDamage, 0.0f) / FMath::Max(AttackInterval, 0.1f);
}

ANightlightEnemy* ANightlightDefender::FindTarget_Implementation()
{
	// The default target is the closest living enemy inside the attack range. The registry replaces a
	// GetAllActorsOfClass search, which is slow when there are many actors (Epic Games, Inc., 2026g).
	const UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>();
	return Registry ? Registry->FindClosestEnemy(GetActorLocation(), AttackRange) : nullptr;
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

Epic Games, Inc., 2026d. FMath. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FMath>
[Accessed 2 October 2026].

Epic Games, Inc., 2026e. Gameplay Timers in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-timers-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026f. UFunctions in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/ufunctions-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026g. UGameplayStatics::GetAllActorsOfClass. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameplayStatics/GetAllActorsOfClass>
[Accessed 29 September 2026].

Epic Games, Inc., 2026h. Unreal Engine Actor Lifecycle. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-actor-lifecycle>
[Accessed 29 September 2026].

Epic Games, Inc., 2026i. UWorld::LineTraceSingleByChannel. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorld/LineTraceSingleByChannel>
[Accessed 2 October 2026].
*/
