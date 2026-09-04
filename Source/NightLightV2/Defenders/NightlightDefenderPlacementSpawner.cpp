#include "NightlightDefenderPlacementSpawner.h"
#include "../ProceduralGeneration/NightlightWorldGenerator.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ANightlightDefenderPlacementSpawner::ANightlightDefenderPlacementSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANightlightDefenderPlacementSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		// The generator builds its grid during BeginPlay, so placement waits until the next tick.
		GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ANightlightDefenderPlacementSpawner::SpawnPlacementAnchors);
	}
}

void ANightlightDefenderPlacementSpawner::SpawnPlacementAnchors()
{
	if (!WorldGenerator)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight defender placements were not spawned because no World Generator is assigned."));
		return;
	}

	if (!PlacementClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight defender placements were not spawned because no Placement Class is assigned."));
		return;
	}

	if (!SpawnedPlacements.IsEmpty())
	{
		return;
	}

	const TArray<FVector> AnchorLocations = WorldGenerator->GetAnchorWorldLocations();
	for (const FVector& AnchorLocation : AnchorLocations)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Placement = GetWorld()->SpawnActor<AActor>(
			PlacementClass,
			AnchorLocation,
			FRotator::ZeroRotator,
			SpawnParameters);

		if (Placement)
		{
			SpawnedPlacements.Add(Placement);
		}
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Nightlight spawned %d defender placements from %d approved Lucid Anchors."),
		SpawnedPlacements.Num(),
		AnchorLocations.Num());
}

/*
References

Epic Games, Inc., 2026a. UWorld::SpawnActor. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorld/SpawnActor>
[Accessed 4 September 2026].
*/
