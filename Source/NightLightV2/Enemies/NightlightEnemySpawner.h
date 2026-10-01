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

	// Caches the generator's routes and moves the Core onto the generated point. The wave director calls this
	// once before its first wave, so it can spawn without the spawner's own timer. False when anything is missing.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Enemy Spawning")
	bool PrepareRoutes();

	// Spawns one enemy at the start of a cached route and gives it that route and the Core. Null when it fails.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Enemy Spawning")
	ANightlightEnemy* SpawnEnemyOnRoute(TSubclassOf<ANightlightEnemy> SpawnClass, int32 RouteIndex);

	// Only routes with at least two points are cached, so every index below this count can be spawned on.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy Spawning")
	int32 GetRouteCount() const { return CachedRouteWorldLocations.Num(); }

	// The cached world points of one route, or an empty list for an invalid index.
	const TArray<FVector>& GetRouteWorldLocations(int32 RouteIndex) const;

	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy Spawning")
	ANightlightDreamCore* GetDreamCore() const { return DreamCore; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy Spawning")
	ANightlightWorldGenerator* GetWorldGenerator() const { return WorldGenerator; }

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

	// Temporary until the wave director replaces it: each spawn uses the next class in this list. When the
	// list is empty the spawner falls back to EnemyClass, so existing levels keep working.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy Spawning")
	TArray<TSubclassOf<ANightlightEnemy>> EnemyClasses;

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
	int32 NextEnemyClassIndex = 0;
	FTimerHandle SpawnTimerHandle;

	// Returns the class for the next spawn and advances the cycle, or null when nothing is assigned.
	TSubclassOf<ANightlightEnemy> TakeNextEnemyClass();
	bool HasAnyEnemyClass() const;
};
