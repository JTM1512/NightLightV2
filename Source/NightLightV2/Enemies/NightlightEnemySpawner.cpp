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
	// (Epic Games, Inc., 2026).
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

/*
References

Epic Games, Inc., 2026. UWorld::SpawnActor. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorld/SpawnActor>
[Accessed 31 August 2026].
*/
