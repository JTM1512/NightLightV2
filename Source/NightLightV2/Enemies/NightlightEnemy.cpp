#include "NightlightEnemy.h"
#include "../Core/NightlightDreamCore.h"
#include "../Defenders/NightlightDefender.h"
#include "../UI/NightlightHealthWidgetUtils.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace
{
	bool ReadNumericProperty(const AActor* const Actor, const FName PropertyName, double& OutValue)
	{
		const FNumericProperty* const Property = FindFProperty<FNumericProperty>(Actor->GetClass(), PropertyName);
		if (!Property)
		{
			return false;
		}

		const void* const ValueAddress = Property->ContainerPtrToValuePtr<void>(Actor);
		OutValue = Property->IsFloatingPoint()
			? Property->GetFloatingPointPropertyValue(ValueAddress)
			: static_cast<double>(Property->GetSignedIntPropertyValue(ValueAddress));
		return true;
	}

	bool WriteNumericProperty(AActor* const Actor, const FName PropertyName, const double Value)
	{
		FNumericProperty* const Property = FindFProperty<FNumericProperty>(Actor->GetClass(), PropertyName);
		if (!Property)
		{
			return false;
		}

		void* const ValueAddress = Property->ContainerPtrToValuePtr<void>(Actor);
		if (Property->IsFloatingPoint())
		{
			Property->SetFloatingPointPropertyValue(ValueAddress, Value);
		}
		else
		{
			Property->SetIntPropertyValue(ValueAddress, FMath::RoundToInt64(Value));
		}
		return true;
	}

	// Fallback for BP_Defender until it is reparented onto ANightlightDefender. Remove this once it is.
	bool ApplyDamageToBlueprintDefender(AActor* const Defender, const double DamageAmount)
	{
		double CurrentHealth = 0.0;
		double MaxHealth = 0.0;
		if (!ReadNumericProperty(Defender, TEXT("CurrentHealth"), CurrentHealth)
			|| !ReadNumericProperty(Defender, TEXT("MaxHealth"), MaxHealth))
		{
			return false;
		}

		const double AppliedDamage = FMath::Min(FMath::Max(DamageAmount, 0.0), FMath::Max(CurrentHealth, 0.0));
		const double NewHealth = FMath::Clamp(CurrentHealth - AppliedDamage, 0.0, FMath::Max(MaxHealth, 0.0));
		if (!WriteNumericProperty(Defender, TEXT("CurrentHealth"), NewHealth))
		{
			return false;
		}

		NightlightHealthWidgetUtils::UpdateWorldHealthWidget(Defender, NewHealth, MaxHealth, AppliedDamage);
		if (NewHealth <= 0.0)
		{
			Defender->Destroy();
		}
		return true;
	}
}

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

void ANightlightEnemy::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (UpdateDefenderCombat(DeltaTime))
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

	const ANightlightDefender* const BaseDefender = Cast<ANightlightDefender>(TargetDefender);
	if (BaseDefender && BaseDefender->IsDead())
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
	FindDefenderTarget();
	return IsValid(TargetDefender);
}

void ANightlightEnemy::FindDefenderTarget()
{
	const float AttackRangeSquared = FMath::Square(FMath::Max(DefenderAttackRange, 0.0f));
	float ClosestDistanceSquared = AttackRangeSquared;

	// Every defender type shares the base class, so one search finds them all
	// (Epic Games, Inc., 2026b).
	TArray<AActor*> Defenders;
	UGameplayStatics::GetAllActorsOfClass(this, ANightlightDefender::StaticClass(), Defenders);

	for (AActor* FoundActor : Defenders)
	{
		const ANightlightDefender* const Defender = Cast<ANightlightDefender>(FoundActor);
		if (!IsValid(Defender) || Defender->IsDead())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Defender->GetActorLocation());
		if (DistanceSquared <= ClosestDistanceSquared)
		{
			TargetDefender = FoundActor;
			ClosestDistanceSquared = DistanceSquared;
		}
	}

	// Fallback for BP_Defender until it is reparented onto ANightlightDefender. Remove this search,
	// DefenderClass and ApplyDamageToBlueprintDefender once it is.
	if (!TargetDefender && DefenderClass)
	{
		Defenders.Reset();
		UGameplayStatics::GetAllActorsOfClass(this, DefenderClass, Defenders);

		for (AActor* Defender : Defenders)
		{
			// Base class defenders were already checked above.
			if (!IsValid(Defender) || Defender->IsA<ANightlightDefender>())
			{
				continue;
			}

			const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Defender->GetActorLocation());
			if (DistanceSquared <= ClosestDistanceSquared)
			{
				TargetDefender = Defender;
				ClosestDistanceSquared = DistanceSquared;
			}
		}
	}

	if (!TargetDefender)
	{
		return;
	}

	// Damage starts immediately, then repeats while the same defender remains in range.
	AttackTargetDefender();
	GetWorldTimerManager().SetTimer(
		DefenderAttackTimerHandle,
		this,
		&ANightlightEnemy::AttackTargetDefender,
		FMath::Max(DefenderAttackInterval, 0.1f),
		true);
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

	const float AppliedDamage = FMath::Max(DefenderAttackDamage, 0.0f);
	if (ANightlightDefender* const Defender = Cast<ANightlightDefender>(TargetDefender))
	{
		Defender->ApplyDamage(AppliedDamage);
	}
	// Fallback for BP_Defender until it is reparented onto ANightlightDefender. Remove this once it is.
	else if (!ApplyDamageToBlueprintDefender(TargetDefender, AppliedDamage))
	{
		// Keep Unreal's standard damage path as a fallback for any other actor class.
		UGameplayStatics::ApplyDamage(TargetDefender, AppliedDamage, nullptr, this, nullptr);
	}

	const ANightlightDefender* const BaseDefender = Cast<ANightlightDefender>(TargetDefender);
	if (!IsValid(TargetDefender) || (BaseDefender && BaseDefender->IsDead()))
	{
		ClearDefenderTarget();
	}
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

Epic Games, Inc., 2026b. UGameplayStatics::GetAllActorsOfClass. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameplayStatics/GetAllActorsOfClass>
[Accessed 29 September 2026].
*/
