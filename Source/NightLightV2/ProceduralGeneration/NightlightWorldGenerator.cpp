#include "NightlightWorldGenerator.h"
#include "Components/SceneComponent.h"
#include "Math/RandomStream.h"

ANightlightWorldGenerator::ANightlightWorldGenerator()
{
	// Generation runs as a single operation, so this Actor does not need Tick.
	PrimaryActorTick.bCanEverTick = false;

	// The root keeps future mesh and debug components under one transform.
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANightlightWorldGenerator::BeginPlay()
{
	Super::BeginPlay();

	// Automatic generation is useful for play. Tests can disable it and call
	// GenerateLogicalGrid directly with controlled settings.
	if (bGenerateOnBeginPlay)
	{
		GenerateLogicalGrid();
	}
}

void ANightlightWorldGenerator::GenerateLogicalGrid()
{
	// Keep both dimensions large enough to support a centre and several routes.
	const int32 Width = FMath::Max(GenerationSettings.GridWidth, 5);
	const int32 Depth = FMath::Max(GenerationSettings.GridDepth, 5);

	// Store the resolved value before any procedural choices are made.
	ActiveSeed = ResolveSessionSeed();

	// One seeded stream keeps the full generation run reproducible
	// (Epic Games, Inc., 2026b).
	FRandomStream RandomStream(ActiveSeed);

	// Move through the noise field so each seed produces a different map.
	const FVector2D NoiseOffset(
		RandomStream.FRandRange(-10000.0f, 10000.0f),
		RandomStream.FRandRange(-10000.0f, 10000.0f));

	// Reserve enough memory for every cell, then rebuild the array from scratch.
	// The mesh will consume this logical grid later.
	Cells.Reset(Width * Depth);

	// Visit every grid coordinate once and store it in row-major order.
	for (int32 Y = 0; Y < Depth; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			// Add the cell directly to the array so its stored order stays stable.
			FNightlightCellData& Cell = Cells.AddDefaulted_GetRef();
			Cell.GridCoordinate = FIntPoint(X, Y);

			// Use continuous noise for the first height pass
			// (Epic Games, Inc., 2026a).
			// Frequency scales grid coordinates into the noise domain.
			const FVector2D NoiseLocation =
				(FVector2D(X, Y) + NoiseOffset) * GenerationSettings.NoiseFrequency;

			// HeightScale converts the normalised noise sample into Unreal units.
			Cell.Height = FMath::PerlinNoise2D(NoiseLocation) * GenerationSettings.HeightScale;
		}
	}

	// Integer division selects one stable centre cell for odd or even grids.
	const FIntPoint CoreCoordinate(Width / 2, Depth / 2);
	FNightlightCellData& CoreCell = Cells[GetCellIndex(CoreCoordinate.X, CoreCoordinate.Y, Width)];

	// Flatten and reserve the Core before paths and placement are added.
	CoreCell.Height = 0.0f;
	CoreCell.Type = ENightlightCellType::Core;
	CoreCell.bBuildable = false;

	// Log the seed now so failed maps can be reproduced during testing.
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Nightlight generated a %dx%d logical grid with seed %d."),
		Width,
		Depth,
		ActiveSeed);
}

int32 ANightlightWorldGenerator::ResolveSessionSeed() const
{
	// Random for normal play; fixed for repeatable testing.
	return GenerationSettings.bUseRandomSeed
		? FMath::Rand()
		: GenerationSettings.Seed;
}

int32 ANightlightWorldGenerator::GetCellIndex(const int32 X, const int32 Y, const int32 Width) const
{
	// Rows are stored one after another: index = row offset + column.
	return Y * Width + X;
}

/*
References

Epic Games, Inc., 2026a. FMath::PerlinNoise2D. [online] Available at:
<https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FMath/PerlinNoise2D>
[Accessed 25 August 2026].

Epic Games, Inc., 2026b. FRandomStream. [online] Available at:
<https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FRandomStream>
[Accessed 25 August 2026].
*/
