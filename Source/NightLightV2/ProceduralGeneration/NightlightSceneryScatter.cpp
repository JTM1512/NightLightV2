#include "NightlightSceneryScatter.h"
#include "NightlightWorldGenerator.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"
#include "TimerManager.h"

ANightlightSceneryScatter::ANightlightSceneryScatter()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANightlightSceneryScatter::BeginPlay()
{
	Super::BeginPlay();

	if (bScatterOnBeginPlay)
	{
		ScatterWhenGenerated();
	}
}

void ANightlightSceneryScatter::ScatterWhenGenerated()
{
	// Actors begin play in no fixed order and the generator builds its cells in its own BeginPlay,
	// so wait one tick at a time until the cells exist (Epic Games, Inc., 2026b).
	if (WorldGenerator && WorldGenerator->GetCells().IsEmpty())
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ANightlightSceneryScatter::ScatterWhenGenerated);
		return;
	}

	ScatterScenery();
}

void ANightlightSceneryScatter::ScatterScenery()
{
	for (UHierarchicalInstancedStaticMeshComponent* Component : SceneryComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	SceneryComponents.Reset();

	if (!WorldGenerator)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight scenery was not scattered because no World Generator is assigned."));
		return;
	}

	const FNightlightGenerationSettings& Settings = WorldGenerator->GetGenerationSettings();
	const int32 Width = FMath::Max(Settings.GridWidth, 5);
	const int32 Depth = FMath::Max(Settings.GridDepth, 5);
	const float CellSize = FMath::Max(Settings.CellSize, 10.0f);
	const TArray<FNightlightCellData>& Cells = WorldGenerator->GetCells();
	if (Cells.Num() != Width * Depth)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight scenery was not scattered because the generator has no cells."));
		return;
	}

	// The map's own seed drives every choice, so the same map always gets the same scenery
	// (Epic Games, Inc., 2026a).
	FRandomStream RandomStream(WorldGenerator->GetActiveSeed());
	const FTransform GeneratorTransform = WorldGenerator->GetActorTransform();

	// Picks one of the meshes and a uniform scale, then moves the grid-space point into the level.
	auto Place = [&](const TArray<TObjectPtr<UStaticMesh>>& Meshes, const FVector& LocalLocation, const float Yaw,
		const FVector2D& ScaleRange)
	{
		if (Meshes.IsEmpty())
		{
			return;
		}

		UStaticMesh* const Mesh = Meshes[RandomStream.RandRange(0, Meshes.Num() - 1)];
		const FTransform LocalTransform(
			FRotator(0.0f, Yaw, 0.0f),
			LocalLocation,
			FVector(RandomStream.FRandRange(ScaleRange.X, ScaleRange.Y)));
		AddSceneryInstance(Mesh, LocalTransform * GeneratorTransform);
	};

	// Returns a random grid-space point within this cell, on the terrain surface. The mesh splits each
	// square into two triangles along the diagonal from (1, 0) to (0, 1), so the height is interpolated
	// across the same triangle and the scenery neither floats nor sinks on slopes.
	auto RandomPointInCell = [&](const FIntPoint& Coordinate)
	{
		const float GridX = FMath::Clamp(Coordinate.X + RandomStream.FRandRange(-0.45f, 0.45f), 0.0f, Width - 1.001f);
		const float GridY = FMath::Clamp(Coordinate.Y + RandomStream.FRandRange(-0.45f, 0.45f), 0.0f, Depth - 1.001f);
		const int32 X = FMath::FloorToInt(GridX);
		const int32 Y = FMath::FloorToInt(GridY);
		const float FracX = GridX - X;
		const float FracY = GridY - Y;
		auto Height = [&](const int32 OffsetX, const int32 OffsetY) { return Cells[(Y + OffsetY) * Width + X + OffsetX].Height; };

		const float Z = FracX + FracY <= 1.0f
			? Height(0, 0) + FracX * (Height(1, 0) - Height(0, 0)) + FracY * (Height(0, 1) - Height(0, 0))
			: Height(1, 1) + (1.0f - FracX) * (Height(0, 1) - Height(1, 1)) + (1.0f - FracY) * (Height(1, 0) - Height(1, 1));
		return FVector(GridX * CellSize, GridY * CellSize, Z);
	};

	const FIntPoint Neighbours[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	for (const FNightlightCellData& Cell : Cells)
	{
		// Anchor pad cells are Ground but not buildable, so this also keeps the whole pad clear.
		if (Cell.Type != ENightlightCellType::Ground || !Cell.bBuildable)
		{
			continue;
		}

		// Skipping cells beside a path, anchor, Rift or the Core keeps tracks and build spots readable.
		bool bNextToReserved = false;
		float LargestRise = 0.0f;
		for (const FIntPoint& Offset : Neighbours)
		{
			const FIntPoint Next = Cell.GridCoordinate + Offset;
			if (Next.X >= 0 && Next.X < Width && Next.Y >= 0 && Next.Y < Depth)
			{
				const FNightlightCellData& Neighbour = Cells[Next.Y * Width + Next.X];
				bNextToReserved |= Neighbour.Type != ENightlightCellType::Ground;
				LargestRise = FMath::Max(LargestRise, FMath::Abs(Neighbour.Height - Cell.Height));
			}
		}
		if (bNextToReserved)
		{
			continue;
		}

		// Steep cells get rocks more often than flat ground.
		if (RandomStream.FRand() < (LargestRise > SteepHeightDifference ? SteepRockChance : RockChance))
		{
			const FVector Location = RandomPointInCell(Cell.GridCoordinate);
			Place(RockMeshes, Location, RandomStream.FRandRange(0.0f, 360.0f), RockScaleRange);
		}
	}

	// The border follows a square around the grid. Each mesh is turned square to its side, facing the map,
	// and pushed out by its own half width plus BorderDistance, so its inner edge meets the map edge.
	const FVector Centre((Width - 1) * CellSize * 0.5f, (Depth - 1) * CellSize * 0.5f, 0.0f);
	for (int32 Index = 0; Index < BorderCount && !BorderMeshes.IsEmpty(); ++Index)
	{
		UStaticMesh* const Mesh = BorderMeshes[RandomStream.RandRange(0, BorderMeshes.Num() - 1)];
		const float Scale = RandomStream.FRandRange(BorderScaleRange.X, BorderScaleRange.Y);
		const float Reach = (Mesh ? Mesh->GetBounds().BoxExtent.X * Scale : 0.0f) + BorderDistance;
		const FVector2D Half(Centre.X + Reach, Centre.Y + Reach);
		const FVector2D Corners[] = { FVector2D(-Half.X, -Half.Y), FVector2D(Half.X, -Half.Y), FVector2D(Half.X, Half.Y), FVector2D(-Half.X, Half.Y) };

		const float Perimeter = 4.0f * (Index + 0.5f) / BorderCount;
		const int32 Side = FMath::FloorToInt(Perimeter) % 4;
		const FVector2D Offset = FMath::Lerp(Corners[Side], Corners[(Side + 1) % 4], Perimeter - FMath::FloorToFloat(Perimeter));
		const FTransform LocalTransform(FRotator(0.0f, 90.0f + 90.0f * Side, 0.0f), Centre + FVector(Offset, 0.0f), FVector(Scale));
		AddSceneryInstance(Mesh, LocalTransform * GeneratorTransform);
	}
}

void ANightlightSceneryScatter::AddSceneryInstance(UStaticMesh* Mesh, const FTransform& WorldTransform)
{
	if (!Mesh)
	{
		return;
	}

	UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
	for (UHierarchicalInstancedStaticMeshComponent* Existing : SceneryComponents)
	{
		if (Existing->GetStaticMesh() == Mesh)
		{
			Component = Existing;
			break;
		}
	}

	if (!Component)
	{
		// One instanced component draws every copy of its mesh together, which keeps every rock and
		// mountain cheap (Epic Games, Inc., 2026c). No collision means nothing here blocks play.
		Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetupAttachment(SceneRoot);
		Component->RegisterComponent();
		SceneryComponents.Add(Component);
	}

	Component->AddInstance(WorldTransform, true);
}

/*
References

Epic Games, Inc., 2026a. FRandomStream. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FRandomStream>
[Accessed 2 October 2026].

Epic Games, Inc., 2026b. FTimerManager::SetTimerForNextTick. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FTimerManager/SetTimerForNextTick>
[Accessed 2 October 2026].

Epic Games, Inc., 2026c. Instanced Static Mesh Component in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine>
[Accessed 2 October 2026].
*/
