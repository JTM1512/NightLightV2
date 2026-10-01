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
	NextRouteIndex = 0;
	NextEnemyClassIndex = 0;

	if (!HasAnyEnemyClass())
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy spawning did not start because no Enemy Class or Enemy Classes are assigned."));
		return;
	}

	if (!PrepareRoutes())
	{
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

bool ANightlightEnemySpawner::PrepareRoutes()
{
	CachedRouteWorldLocations.Reset();

	if (!WorldGenerator)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy routes were not prepared because no World Generator is assigned."));
		return false;
	}

	if (!DreamCore)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy routes were not prepared because no Dream Core is assigned."));
		return false;
	}

	FVector CoreWorldLocation;
	if (!WorldGenerator->GetCoreWorldLocation(CoreWorldLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy routes were not prepared because the generator has no valid Core position."));
		return false;
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
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy routes were not prepared because the generator has no complete routes."));
		return false;
	}

	return true;
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
	if (CachedRouteWorldLocations[RouteIndex].Num() < 2)
	{
		StopSpawning();
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy spawning stopped because a cached route became incomplete."));
		return;
	}

	if (SpawnEnemyOnRoute(TakeNextEnemyClass(), RouteIndex))
	{
		NextRouteIndex = (RouteIndex + 1) % CachedRouteWorldLocations.Num();
	}
}

ANightlightEnemy* ANightlightEnemySpawner::SpawnEnemyOnRoute(
	const TSubclassOf<ANightlightEnemy> SpawnClass,
	const int32 RouteIndex)
{
	if (!SpawnClass || !CachedRouteWorldLocations.IsValidIndex(RouteIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight could not spawn an enemy because it has no class or route %d is not prepared."), RouteIndex);
		return nullptr;
	}

	const TArray<FVector>& RoutePoints = CachedRouteWorldLocations[RouteIndex];
	if (RoutePoints.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight could not spawn an enemy because route %d is incomplete."), RouteIndex);
		return nullptr;
	}

	// AlwaysSpawn places the enemy on its Rift even when it overlaps the terrain
	// (Epic Games, Inc., 2026b).
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANightlightEnemy* Enemy = GetWorld()->SpawnActor<ANightlightEnemy>(
		SpawnClass,
		RoutePoints[0],
		FRotator::ZeroRotator,
		SpawnParameters);

	if (!Enemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight could not spawn the selected enemy class at route %d."), RouteIndex);
		return nullptr;
	}

	Enemy->AssignDreamCore(DreamCore);
	Enemy->AssignRoute(RoutePoints);
	return Enemy;
}

const TArray<FVector>& ANightlightEnemySpawner::GetRouteWorldLocations(const int32 RouteIndex) const
{
	// A reference needs something to point at, so an invalid index gets this shared empty list.
	static const TArray<FVector> EmptyRoute;
	return CachedRouteWorldLocations.IsValidIndex(RouteIndex) ? CachedRouteWorldLocations[RouteIndex] : EmptyRoute;
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
