#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightlightSceneryScatter.generated.h"

class ANightlightWorldGenerator;
class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMesh;

// Scatters rocks over open ground and rings the map with mountains, all from the map seed.
// Scenery has no collision, so it never blocks enemies, defender placement clicks or the camera.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightSceneryScatter : public AActor
{
	GENERATED_BODY()

public:
	ANightlightSceneryScatter();

	// Clears any earlier scenery and scatters it again from the generator's current cells and seed.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Scenery")
	void ScatterScenery();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// The generator whose cells and seed decide where scenery goes.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Nightlight|Scenery")
	TObjectPtr<ANightlightWorldGenerator> WorldGenerator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Scenery")
	TArray<TObjectPtr<UStaticMesh>> RockMeshes;

	// Large meshes for the ring outside the grid that gives the map a horizon.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Scenery")
	TArray<TObjectPtr<UStaticMesh>> BorderMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Scenery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RockChance = 0.06f;

	// Rock chance on steep cells, so slopes look rockier than flat ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Scenery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SteepRockChance = 0.35f;

	// A cell is steep when a neighbour is more than this much higher or lower.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Scenery", meta = (ClampMin = "0.0"))
	float SteepHeightDifference = 60.0f;

	// Uniform scale ranges, sized for the desert rocks and the 12,500-unit desert mountains.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Scenery")
	FVector2D RockScaleRange = FVector2D(0.8f, 1.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Scenery")
	FVector2D BorderScaleRange = FVector2D(0.15f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Scenery", meta = (ClampMin = "0"))
	int32 BorderCount = 24;

	// Gap between each border mesh's bounds and the map edge. Negative values tuck the mesh's low outer
	// slopes under the map edge, because a mountain's bounds are wider than its visible base.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Scenery")
	float BorderDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Scenery")
	bool bScatterOnBeginPlay = true;

private:
	friend class FNightlightSceneryScatterTest;

	// Waits a tick at a time until the generator has filled its cells, then scatters.
	void ScatterWhenGenerated();

	// Finds or creates the instance component for one mesh and adds a world-space instance to it.
	void AddSceneryInstance(UStaticMesh* Mesh, const FTransform& WorldTransform);

	// One instanced component per mesh, created at runtime.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> SceneryComponents;
};
