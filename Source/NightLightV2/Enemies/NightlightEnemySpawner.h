#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "NightlightEnemySpawner.generated.h"

class ANightlightEnemy;
class ANightlightDreamCore;
class ANightlightWorldGenerator;
class USceneComponent;

// Spawns enemies using the routes already created by the world generator.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	ANightlightEnemySpawner();

	UFUNCTION(BlueprintCallable, Category = "Nightlight|Enemy Spawning")
	void StartSpawning();

	UFUNCTION(BlueprintCallable, Category = "Nightlight|Enemy Spawning")
	void StopSpawning();

	UFUNCTION(BlueprintCallable, Category = "Nightlight|Enemy Spawning")
	void SpawnNextEnemy();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// Select the exact generator that owns the routes used by this spawner.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Nightlight|Enemy Spawning")
	TObjectPtr<ANightlightWorldGenerator> WorldGenerator;

	// Assign the Core placed in the level. Every spawned enemy receives this reference.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Nightlight|Enemy Spawning")
	TObjectPtr<ANightlightDreamCore> DreamCore;

	// This can use the C++ enemy or a Blueprint child with its own mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy Spawning")
	TSubclassOf<ANightlightEnemy> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy Spawning", meta = (ClampMin = "0.1"))
	float SpawnInterval = 3.0f;

	// Give the player time to inspect the generated map and place their first defenders.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy Spawning", meta = (ClampMin = "0.0"))
	float InitialSpawnDelay = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy Spawning")
	bool bSpawnOnBeginPlay = true;

private:
	// Store the world routes once, then give each enemy its own copy.
	TArray<TArray<FVector>> CachedRouteWorldLocations;
	int32 NextRouteIndex = 0;
	FTimerHandle SpawnTimerHandle;
};
