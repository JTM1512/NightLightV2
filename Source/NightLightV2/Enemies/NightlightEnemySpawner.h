#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightlightEnemySpawner.generated.h"

class ANightlightEnemy;
class ANightlightDreamCore;
class ANightlightWorldGenerator;
class USceneComponent;

// Holds the routes the world generator created and spawns one enemy on a route whenever the wave director asks.
// It never spawns by itself, so the director decides every enemy and when it arrives.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	ANightlightEnemySpawner();

	// Caches the generator's routes and moves the Core onto the generated point. The wave director calls this
	// once before its first wave. False when anything is missing.
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// Select the exact generator that owns the routes used by this spawner.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Nightlight|Enemy Spawning")
	TObjectPtr<ANightlightWorldGenerator> WorldGenerator;

	// Assign the Core placed in the level. Every spawned enemy receives this reference.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Nightlight|Enemy Spawning")
	TObjectPtr<ANightlightDreamCore> DreamCore;

private:
	// Store the world routes once, then give each enemy its own copy.
	TArray<TArray<FVector>> CachedRouteWorldLocations;
};
