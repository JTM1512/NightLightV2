#include "NightlightEnemy.h"
#include "Components/SceneComponent.h"

ANightlightEnemy::ANightlightEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANightlightEnemy::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	MoveAlongRoute(DeltaTime);
}

void ANightlightEnemy::AssignRoute(const TArray<FVector>& RoutePoints)
{
	AssignedRoutePoints = RoutePoints;
	CurrentWaypointIndex = INDEX_NONE;
	bHasReachedCore = false;
	SetActorTickEnabled(false);

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

void ANightlightEnemy::MoveAlongRoute(const float DeltaTime)
{
	if (bHasReachedCore || !AssignedRoutePoints.IsValidIndex(CurrentWaypointIndex))
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
	bHasReachedCore = true;
	SetActorTickEnabled(false);
	OnCoreReached();
}

/*
References

Epic Games, Inc., 2026a. FMath::VInterpConstantTo. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FMath/VInterpConstantTo>
[Accessed 31 August 2026].
*/
