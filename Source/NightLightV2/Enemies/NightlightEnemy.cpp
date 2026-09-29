#include "NightlightEnemy.h"
#include "../Core/NightlightDreamCore.h"
#include "../Defenders/NightlightDefender.h"
#include "../Systems/NightlightActorRegistrySubsystem.h"
#include "../UI/NightlightHealthWidgetUtils.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"

ANightlightEnemy::ANightlightEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANightlightEnemy::BeginPlay()
{
	Super::BeginPlay();

	// The registry lets defenders, the Core and the later wave director find this enemy without an actor search.
	if (UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>())
	{
		Registry->RegisterEnemy(this);
	}

	// Blueprint enemy types can change MaxHealth, so copy it when the game starts.
	MaxHealth = FMath::Max(MaxHealth, 0.0f);
	CurrentHealth = MaxHealth;
	bIsDead = false;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	NightlightHealthWidgetUtils::UpdateWorldHealthWidget(this, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void ANightlightEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EndPlay runs when the enemy dies, reaches the Core or the level ends, so this one call covers
	// every way an enemy leaves play (Epic Games, Inc., 2026e).
	if (UWorld* const World = GetWorld())
	{
		if (UNightlightActorRegistrySubsystem* const Registry = World->GetSubsystem<UNightlightActorRegistrySubsystem>())
		{
			Registry->UnregisterEnemy(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ANightlightEnemy::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (UpdateDefenderCombat(DeltaTime) && ShouldStopForDefender())
	{
		return;
	}

	MoveAlongRoute(DeltaTime);
}

float ANightlightEnemy::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* const EventInstigator,
	AActor* const DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	ApplyDamage(AppliedDamage);
	return AppliedDamage;
}

void ANightlightEnemy::AssignRoute(const TArray<FVector>& RoutePoints)
{
	AssignedRoutePoints = RoutePoints;
	CurrentWaypointIndex = INDEX_NONE;
	bHasReachedCore = false;
	SetActorTickEnabled(false);
	if (bIsDead)
	{
		return;
	}

	if (AssignedRoutePoints.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy received an incomplete route and will remain stopped."));
		return;
	}

	// The first point is the Rift spawn position, so movement starts at point one.
	SetActorLocation(AssignedRoutePoints[0]);
	CurrentWaypointIndex = 1;
	SetActorTickEnabled(true);
}

void ANightlightEnemy::AssignDreamCore(ANightlightDreamCore* const InDreamCore)
{
	DreamCore = InDreamCore;
}

void ANightlightEnemy::ApplyDamage(const float DamageAmount)
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

void ANightlightEnemy::MoveAlongRoute(const float DeltaTime)
{
	if (bIsDead || bHasReachedCore || !AssignedRoutePoints.IsValidIndex(CurrentWaypointIndex))
	{
		SetActorTickEnabled(false);
		return;
	}

	const FVector TargetLocation = AssignedRoutePoints[CurrentWaypointIndex];
	const FVector NewLocation = FMath::VInterpConstantTo(
		GetActorLocation(),
		TargetLocation,
		DeltaTime,
		FMath::Max(MovementSpeed, 0.0f));

	// VInterpConstantTo stops at the target instead of moving past the waypoint
	// (Epic Games, Inc., 2026a).
	SetActorLocation(NewLocation);

	const float AcceptanceDistance = FMath::Max(WaypointAcceptanceDistance, 0.0f);
	if (FVector::DistSquared(NewLocation, TargetLocation) <= FMath::Square(AcceptanceDistance))
	{
		SetActorLocation(TargetLocation);
		ReachNextWaypoint();
	}
}

bool ANightlightEnemy::UpdateDefenderCombat(const float DeltaTime)
{
	if (bIsDead || bHasReachedCore)
	{
		ClearDefenderTarget();
		return false;
	}

	if (IsValid(TargetDefender) && TargetDefender->IsDead())
	{
		ClearDefenderTarget();
	}

	const float AttackRangeSquared = FMath::Square(FMath::Max(DefenderAttackRange, 0.0f));
	if (IsValid(TargetDefender))
	{
		if (FVector::DistSquared(GetActorLocation(), TargetDefender->GetActorLocation()) <= AttackRangeSquared)
		{
			return true;
		}

		ClearDefenderTarget();
	}

	TimeUntilDefenderSearch -= DeltaTime;
	if (TimeUntilDefenderSearch > 0.0f)
	{
		return false;
	}

	// A short search delay keeps this simple without scanning every defender every frame.
	TimeUntilDefenderSearch = 0.25f;
	TargetDefender = FindDefenderTarget();
	if (!IsValid(TargetDefender))
	{
		TargetDefender = nullptr;
		return false;
	}

	// Damage starts immediately, then repeats while the same defender remains in range
	// (Epic Games, Inc., 2026b).
	AttackTargetDefender();
	if (!IsValid(TargetDefender))
	{
		return false;
	}

	GetWorldTimerManager().SetTimer(
		DefenderAttackTimerHandle,
		this,
		&ANightlightEnemy::AttackTargetDefender,
		FMath::Max(DefenderAttackInterval, 0.1f),
		true);
	return true;
}

ANightlightDefender* ANightlightEnemy::FindDefenderTarget()
{
	const float AttackRangeSquared = FMath::Square(FMath::Max(DefenderAttackRange, 0.0f));
	float ClosestDistanceSquared = AttackRangeSquared;
	ANightlightDefender* ClosestDefender = nullptr;

	// Every defender type shares the base class, so one search finds them all
	// (Epic Games, Inc., 2026d).
	TArray<AActor*> Defenders;
	UGameplayStatics::GetAllActorsOfClass(this, ANightlightDefender::StaticClass(), Defenders);

	for (AActor* FoundActor : Defenders)
	{
		ANightlightDefender* const Defender = Cast<ANightlightDefender>(FoundActor);
		if (!IsValid(Defender) || Defender->IsDead())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Defender->GetActorLocation());
		if (DistanceSquared <= ClosestDistanceSquared)
		{
			ClosestDefender = Defender;
			ClosestDistanceSquared = DistanceSquared;
		}
	}

	return ClosestDefender;
}

void ANightlightEnemy::AttackTargetDefender()
{
	if (bIsDead || bHasReachedCore || !IsValid(TargetDefender))
	{
		ClearDefenderTarget();
		return;
	}

	const float AttackRangeSquared = FMath::Square(FMath::Max(DefenderAttackRange, 0.0f));
	if (FVector::DistSquared(GetActorLocation(), TargetDefender->GetActorLocation()) > AttackRangeSquared)
	{
		ClearDefenderTarget();
		return;
	}

	// The event fires before the hit so the Blueprint still has a valid defender if this hit
	// destroys it. Blueprints implement it without any C++ body (Epic Games, Inc., 2026c).
	OnAttackDefender(TargetDefender);
	AttackDefender(TargetDefender);
	if (!IsValid(TargetDefender) || TargetDefender->IsDead())
	{
		ClearDefenderTarget();
	}
}

void ANightlightEnemy::AttackDefender(ANightlightDefender* const Defender)
{
	if (IsValid(Defender))
	{
		Defender->ApplyDamage(FMath::Max(DefenderAttackDamage, 0.0f));
	}
}

bool ANightlightEnemy::ShouldStopForDefender() const
{
	return true;
}

void ANightlightEnemy::ClearDefenderTarget()
{
	GetWorldTimerManager().ClearTimer(DefenderAttackTimerHandle);
	TargetDefender = nullptr;
}

void ANightlightEnemy::ReachNextWaypoint()
{
	++CurrentWaypointIndex;
	if (CurrentWaypointIndex >= AssignedRoutePoints.Num())
	{
		HandleCoreReached();
	}
}

void ANightlightEnemy::HandleCoreReached()
{
	if (bIsDead || bHasReachedCore)
	{
		return;
	}

	// Set this before the damage and event so another Tick cannot repeat the arrival.
	bHasReachedCore = true;
	SetActorTickEnabled(false);
	ClearDefenderTarget();

	if (IsValid(DreamCore) && !DreamCore->IsCoreDestroyed())
	{
		DreamCore->ApplyCoreDamage(FMath::Max(CoreDamage, 0.0f));
	}

	OnCoreReached();
	Destroy();
}

void ANightlightEnemy::Die()
{
	if (bIsDead)
	{
		return;
	}

	// Stop movement before the event because a dead enemy must never reach the Core.
	bIsDead = true;
	SetActorTickEnabled(false);
	ClearDefenderTarget();
	AwardDeathTokens(TokensOnDeath);
	OnEnemyDied.Broadcast();
	Destroy();
}

/*
References

Epic Games, Inc., 2026a. FMath::VInterpConstantTo. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FMath/VInterpConstantTo>
[Accessed 31 August 2026].

Epic Games, Inc., 2026b. Gameplay Timers in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-timers-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026c. UFunctions in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/ufunctions-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026d. UGameplayStatics::GetAllActorsOfClass. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameplayStatics/GetAllActorsOfClass>
[Accessed 29 September 2026].

Epic Games, Inc., 2026e. Unreal Engine Actor Lifecycle. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-actor-lifecycle>
[Accessed 29 September 2026].
*/
