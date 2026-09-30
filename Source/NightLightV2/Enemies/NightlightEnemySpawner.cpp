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
		if (InitialSpawnDelay > 0.0f)
		{
			// The setup window starts after generation, giving the player time to read the map and place defenders.
			GetWorldTimerManager().SetTimer(
				SpawnTimerHandle,
				this,
				&ANightlightEnemySpawner::StartSpawning,
				InitialSpawnDelay,
				false);
		}
		else
		{
			// Still wait one tick so the generator can finish creating its routes first.
			GetWorldTimerManager().SetTimerForNextTick(
				this,
				&ANightlightEnemySpawner::StartSpawning);
		}
	}
}

void ANightlightEnemySpawner::StartSpawning()
{
	StopSpawning();
	CachedRouteWorldLocations.Reset();
	NextRouteIndex = 0;
	NextEnemyClassIndex = 0;

	if (!WorldGenerator)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy spawning did not start because no World Generator is assigned."));
		return;
	}

	if (!HasAnyEnemyClass())
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy spawning did not start because no Enemy Class or Enemy Classes are assigned."));
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
	if (!HasAnyEnemyClass() || CachedRouteWorldLocations.IsEmpty())
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

	// AlwaysSpawn places the enemy on its Rift even when it overlaps the terrain
	// (Epic Games, Inc., 2026b).
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANightlightEnemy* Enemy = GetWorld()->SpawnActor<ANightlightEnemy>(
		TakeNextEnemyClass(),
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

bool ANightlightEnemySpawner::HasAnyEnemyClass() const
{
	if (EnemyClass)
	{
		return true;
	}

	// A list of empty entries counts as unassigned (Epic Games, Inc., 2026a).
	return EnemyClasses.ContainsByPredicate([](const TSubclassOf<ANightlightEnemy>& Class) { return Class != nullptr; });
}

TSubclassOf<ANightlightEnemy> ANightlightEnemySpawner::TakeNextEnemyClass()
{
	// Cycle through the list in order, skipping empty entries. The wave director will replace this.
	for (int32 Attempt = 0; Attempt < EnemyClasses.Num(); ++Attempt)
	{
		const int32 ClassIndex = NextEnemyClassIndex % EnemyClasses.Num();
		NextEnemyClassIndex = (ClassIndex + 1) % EnemyClasses.Num();
		if (EnemyClasses[ClassIndex])
		{
			return EnemyClasses[ClassIndex];
		}
	}

	return EnemyClass;
}

/*
References

Epic Games, Inc., 2026a. TArray::ContainsByPredicate. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/TArray/ContainsByPredicate>
[Accessed 30 September 2026].

Epic Games, Inc., 2026b. UWorld::SpawnActor. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorld/SpawnActor>
[Accessed 31 August 2026].
*/
