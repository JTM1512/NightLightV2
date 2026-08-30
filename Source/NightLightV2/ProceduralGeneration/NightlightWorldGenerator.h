#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightlightGenerationTypes.h"
#include "NightlightWorldGenerator.generated.h"

class USceneComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
struct FRandomStream;

// Internal arrays used to submit and verify one generated terrain section.
struct FNightlightTerrainMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
};

// Builds the logical map and converts it into one runtime terrain surface.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightWorldGenerator : public AActor
{
	GENERATED_BODY()

public:
	// Sets up the Actor without enabling per-frame Tick work.
	ANightlightWorldGenerator();

	// Rebuilds the grid from the current settings.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Generation")
	void GenerateLogicalGrid();

	// Exposes the generated cells for mesh building, validation and debug drawing.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Generation")
	const TArray<FNightlightCellData>& GetCells() const { return Cells; }

	// Exposes ordered Rift-to-Core routes for later mesh and enemy systems.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Generation")
	const TArray<FNightlightRouteData>& GetRoutes() const { return Routes; }

	// Exposes the approved logical cells as a data-only Blueprint node because
	// reading the generated contract does not change it (Epic Games, Inc., 2026f).
	UFUNCTION(BlueprintPure, Category = "Nightlight|Generation|Anchors")
	const TArray<FIntPoint>& GetAnchorCoordinates() const { return AnchorCoordinates; }

	// Converts every anchor into its terrain-aligned position in the level.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Generation|Anchors")
	TArray<FVector> GetAnchorWorldLocations() const;

	// Exposes the resolved seed so a generated map can be reproduced.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Generation")
	int32 GetActiveSeed() const { return ActiveSeed; }

protected:
	// Starts generation once the Actor enters play, when enabled.
	virtual void BeginPlay() override;

	// Gives the generator a stable transform in the level.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// Owns the generated terrain surface and its runtime collision.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	// Groups the designer-facing generation values in one place.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	FNightlightGenerationSettings GenerationSettings;

	// Allows tests or setup code to generate the grid manually.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	bool bGenerateOnBeginPlay = true;

	// Allows designers to replace the terrain appearance without changing C++.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Mesh")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	// Creates query and physics collision from the generated terrain triangles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Mesh")
	bool bCreateTerrainCollision = true;

	// Records the seed actually used by the current map.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Generation")
	int32 ActiveSeed = 0;

	// Stores logical map data before any visible geometry is created.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Generation")
	TArray<FNightlightCellData> Cells;

	// Keeps every route separate and ordered, even though they share the Core.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Generation")
	TArray<FNightlightRouteData> Routes;

	// Stores the valid terrain cells offered to the teammate's defender system.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Generation|Anchors")
	TArray<FIntPoint> AnchorCoordinates;

private:
	friend class FNightlightRouteGenerationTest;

	// Keeps visible geometry and collision synchronized with the logical grid.
	void RebuildTerrainMesh();

	// Selects either a random session seed or the configured debug seed.
	int32 ResolveSessionSeed() const;

	// Converts a 2D coordinate into the flat Cells array index.
	int32 GetCellIndex(int32 X, int32 Y, int32 Width) const;

	// Selects distinct edges and builds their routes after terrain heights exist.
	void GenerateRoutes(int32 Width, int32 Depth, const FIntPoint& CoreCoordinate, FRandomStream& RandomStream);

	// Keeps each entrance away from corners and in a separate map quadrant.
	FIntPoint SelectRouteEntrance(int32 EdgeIndex, int32 Width, int32 Depth, FRandomStream& RandomStream) const;

	// Creates a shortest orthogonally connected route through a unique Core approach.
	TArray<FIntPoint> BuildRouteToCore(
		const FIntPoint& Start,
		const FIntPoint& CoreCoordinate,
		const FIntPoint& CoreApproach,
		FRandomStream& RandomStream) const;

	// Marks route cells and blends their heights into a traversable slope.
	void ApplyRouteToGrid(FNightlightRouteData& Route, const FIntPoint& CoreCoordinate, int32 Width);

	// Checks route count, ordering, connectivity, cell roles and overlap.
	bool ValidateGeneratedRoutes(const FIntPoint& CoreCoordinate, int32 Width, int32 Depth) const;

	// Selects repeatable Ground cells after routes have reserved their space.
	void GenerateAnchors(int32 Width, int32 Depth, FRandomStream& RandomStream);

	// Converts the completed grid into deterministic render and collision arrays.
	bool BuildTerrainMeshData(FNightlightTerrainMeshData& OutMeshData) const;

	// Maps logical cell roles to material-readable vertex colours.
	static FLinearColor GetCellVertexColor(ENightlightCellType CellType);
};
