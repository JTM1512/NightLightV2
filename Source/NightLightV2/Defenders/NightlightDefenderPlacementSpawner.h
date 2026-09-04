#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightlightDefenderPlacementSpawner.generated.h"

class ANightlightWorldGenerator;
class USceneComponent;

// Creates one defender placement Actor for every approved Lucid Anchor.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightDefenderPlacementSpawner : public AActor
{
	GENERATED_BODY()

public:
	ANightlightDefenderPlacementSpawner();

	UFUNCTION(BlueprintCallable, Category = "Nightlight|Defender Placement")
	void SpawnPlacementAnchors();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// Select the exact generator that owns the approved anchor positions.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Nightlight|Defender Placement")
	TObjectPtr<ANightlightWorldGenerator> WorldGenerator;

	// Assign BP_DefenderPlacement so its existing click and occupancy logic is reused.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender Placement")
	TSubclassOf<AActor> PlacementClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender Placement")
	bool bSpawnOnBeginPlay = true;

private:
	// Keeping the spawned Actors prevents this spawner from creating duplicate anchors.
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedPlacements;
};
