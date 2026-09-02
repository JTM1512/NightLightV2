#include "NightlightEnemySpawner.h"
#include "NightlightEnemy.h"
#include "../Core/NightlightDreamCore.h"
#include "../ProceduralGeneration/NightlightWorldGenerator.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

ANightlightEnemySpawner::ANightlightEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANightlightEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		// Wait one tick so the generator can finish creating its routes first.
		GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ANightlightEnemySpawner::StartSpawning);
	}
}

void ANightlightEnemySpawner::StartSpawning()
{
	StopSpawning();
	CachedRouteWorldLocations.Reset();
	NextRouteIndex = 0;

	if (!WorldGenerator)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy spawning did not start because no World Generator is assigned."));
		return;
	}

	if (!EnemyClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy spawning did not start because no Enemy Class is assigned."));
		return;
	}

	if (!DreamCore)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy spawning did not start because no Dream Core is assigned."));
		return;
	}

	FVector CoreWorldLocation;
	if (!WorldGenerator->GetCoreWorldLocation(CoreWorldLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy spawning did not start because the generator has no valid Core position."));
		return;
	}

	// Keep the placed Core on the same generated point where every route ends.
	DreamCore->SetActorLocation(CoreWorldLocation);

	const TArray<FNightlightRouteData>& Routes = WorldGenerator->GetRoutes();
	for (int32 RouteIndex = 0; RouteIndex < Routes.Num(); ++RouteIndex)
	{
		TArray<FVector> RouteWorldLocations = WorldGenerator->GetRouteWorldLocations(RouteIndex);
		if (RouteWorldLocations.Num() >= 2)
		{
			CachedRouteWorldLocations.Add(MoveTemp(RouteWorldLocations));
		}
	}

	if (CachedRouteWorldLocations.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy spawning did not start because the generator has no complete routes."));
		return;
	}

	// Spawn the first enemy now, then let the timer handle the rest.
	SpawnNextEnemy();
	GetWorldTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&ANightlightEnemySpawner::SpawnNextEnemy,
		FMath::Max(SpawnInterval, 0.1f),
		true);
}

void ANightlightEnemySpawner::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
}

void ANightlightEnemySpawner::SpawnNextEnemy()
{
	if (!EnemyClass || CachedRouteWorldLocations.IsEmpty())
	{
		StopSpawning();
		return;
	}

	const int32 RouteIndex = NextRouteIndex % CachedRouteWorldLocations.Num();
	const TArray<FVector>& RoutePoints = CachedRouteWorldLocations[RouteIndex];
	if (RoutePoints.Num() < 2)
	{
		StopSpawning();
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy spawning stopped because a cached route became incomplete."));
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANightlightEnemy* Enemy = GetWorld()->SpawnActor<ANightlightEnemy>(
		EnemyClass,
		RoutePoints[0],
		FRotator::ZeroRotator,
		SpawnParameters);

	if (!Enemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight could not spawn the selected enemy class at route %d."), RouteIndex);
		return;
	}

	Enemy->AssignDreamCore(DreamCore);
	Enemy->AssignRoute(RoutePoints);
	NextRouteIndex = (RouteIndex + 1) % CachedRouteWorldLocations.Num();
}

/*
References

Epic Games, Inc., 2026a. UWorld::SpawnActor. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorld/SpawnActor>
[Accessed 31 August 2026].
*/
