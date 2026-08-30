#pragma once

#include "CoreMinimal.h"
#include "NightlightGenerationTypes.generated.h"

// Roles used by the logical map before the runtime mesh is built.
UENUM(BlueprintType)
enum class ENightlightCellType : uint8
{
	// Normal terrain that can later support scenery or a placement anchor.
	Ground,

	// Flattened terrain reserved for enemy movement.
	Path,

	// Central non-buildable cell reserved for the Dream Core.
	Core,

	// Route entrance used by an enemy spawner.
	Rift,

	// Predetermined position where the player may place a Guardian.
	PlacementAnchor
};

// Stores the generated state of one grid cell.
USTRUCT(BlueprintType)
struct NIGHTLIGHTV2_API FNightlightCellData
{
	GENERATED_BODY()

	// Integer position in the logical grid. This is not a world-space location.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation")
	FIntPoint GridCoordinate = FIntPoint::ZeroValue;

	// Final Z value used when the runtime mesh creates this cell.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation")
	float Height = 0.0f;

	// Controls which gameplay rules apply to this cell.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation")
	ENightlightCellType Type = ENightlightCellType::Ground;

	// Prevents placement on paths, the Core and other reserved cells.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation")
	bool bBuildable = true;
};

// Stores one complete enemy route in travel order from its Rift to the Core.
USTRUCT(BlueprintType)
struct NIGHTLIGHTV2_API FNightlightRouteData
{
	GENERATED_BODY()

	// Identifies the edge cell where this route and its future spawner begin.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation")
	FIntPoint RiftCoordinate = FIntPoint::ZeroValue;

	// Ordered logical coordinates used later by enemy movement and route visuals.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation")
	TArray<FIntPoint> CellsToCore;
};

// Keeps generation values editable without burying them in the algorithm.
USTRUCT(BlueprintType)
struct NIGHTLIGHTV2_API FNightlightGenerationSettings
{
	GENERATED_BODY()

	// Number of cells across the X axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "5"))
	int32 GridWidth = 31;

	// Number of cells across the Y axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "5"))
	int32 GridDepth = 31;

	// Distance between neighbouring cell vertices in Unreal units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "10.0"))
	float CellSize = 200.0f;

	// Controls how often the terrain material repeats across neighbouring cells.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Mesh", meta = (ClampMin = "0.001"))
	float UVScale = 0.25f;

	// Controls how quickly terrain height changes between cells.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0.0001"))
	float NoiseFrequency = 0.075f;

	// Controls the maximum vertical strength of the noise result.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0.0"))
	float HeightScale = 300.0f;

	// Number of distinct map edges that receive a route and Rift.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "3", ClampMax = "4"))
	int32 RouteCount = 3;

	// Debug seed used when random session seeds are disabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 Seed = 1337;

	// Creates a new map in normal play while keeping fixed-seed testing available.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	bool bUseRandomSeed = true;
};
