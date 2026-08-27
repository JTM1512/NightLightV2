#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightlightGenerationTypes.h"
#include "NightlightWorldGenerator.generated.h"

class USceneComponent;

// Builds and owns the logical map. Mesh construction comes later.
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

	// Exposes the resolved seed so a generated map can be reproduced.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Generation")
	int32 GetActiveSeed() const { return ActiveSeed; }

protected:
	// Starts generation once the Actor enters play, when enabled.
	virtual void BeginPlay() override;

	// Gives the generator a stable transform in the level.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// Groups the designer-facing generation values in one place.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	FNightlightGenerationSettings GenerationSettings;

	// Allows tests or setup code to generate the grid manually.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	bool bGenerateOnBeginPlay = true;

	// Records the seed actually used by the current map.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Generation")
	int32 ActiveSeed = 0;

	// Stores logical map data before any visible geometry is created.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Generation")
	TArray<FNightlightCellData> Cells;

private:
	// Selects either a random session seed or the configured debug seed.
	int32 ResolveSessionSeed() const;

	// Converts a 2D coordinate into the flat Cells array index.
	int32 GetCellIndex(int32 X, int32 Y, int32 Width) const;
};
