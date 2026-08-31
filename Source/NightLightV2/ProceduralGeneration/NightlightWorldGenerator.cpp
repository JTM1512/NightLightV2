#include "NightlightWorldGenerator.h"
#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"
#include "Math/RandomStream.h"

ANightlightWorldGenerator::ANightlightWorldGenerator()
{
	// Generation runs as a single operation, so this Actor does not need Tick.
	PrimaryActorTick.bCanEverTick = false;

	// The root keeps future mesh and debug components under one transform.
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// One shared heightfield section is enough for the current 31x31 map.
	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	TerrainMesh->SetupAttachment(SceneRoot);
	TerrainMesh->bUseComplexAsSimpleCollision = true;
	TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TerrainMesh->SetCollisionObjectType(ECC_WorldStatic);
	TerrainMesh->SetCollisionResponseToAllChannels(ECR_Block);
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

	// One seeded stream controls terrain, Rift selection and route turns. A fixed
	// seed therefore repeats the complete logical map (Epic Games, Inc., 2026c;
	// Unreal Engine, 2015).
	FRandomStream RandomStream(ActiveSeed);

	// Move through the noise field so each seed produces a different map.
	const FVector2D NoiseOffset(
		RandomStream.FRandRange(-10000.0f, 10000.0f),
		RandomStream.FRandRange(-10000.0f, 10000.0f));

	// Reserve enough memory for every cell, then rebuild the array from scratch.
	// The next increment will convert this completed grid into visible geometry.
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
			// (Epic Games, Inc., 2026b).
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

	// Routes depend on the completed height field and always finish at the Core.
	GenerateRoutes(Width, Depth, CoreCoordinate, RandomStream);
	const bool bRoutesValid = ValidateGeneratedRoutes(CoreCoordinate, Width, Depth);

	// Anchors are the final logical pass. Waiting for valid routes means defender
	// positions can never reserve cells that enemy movement already needs.
	if (bRoutesValid)
	{
		GenerateAnchors(Width, Depth, RandomStream);
		RebuildTerrainMesh();

		UE_LOG(
			LogTemp,
			Log,
			TEXT("Nightlight generated a %dx%d logical grid with seed %d. Routes: %d. Anchors: %d. Route validation: passed."),
			Width,
			Depth,
			ActiveSeed,
			Routes.Num(),
			AnchorCoordinates.Num());
	}
	else
	{
		AnchorCoordinates.Reset();

		// A failed logical result must not leave an older valid surface visible.
		if (!IsTemplate() && TerrainMesh)
		{
			TerrainMesh->ClearAllMeshSections();
		}

		UE_LOG(
			LogTemp,
			Error,
			TEXT("Nightlight route validation failed for seed %d."),
			ActiveSeed);
	}
}

TArray<FVector> ANightlightWorldGenerator::GetRouteWorldLocations(const int32 RouteIndex) const
{
	TArray<FVector> WorldLocations;
	if (!Routes.IsValidIndex(RouteIndex))
	{
		return WorldLocations;
	}

	const int32 Width = FMath::Max(GenerationSettings.GridWidth, 5);
	const int32 Depth = FMath::Max(GenerationSettings.GridDepth, 5);
	const float CellSize = FMath::Max(GenerationSettings.CellSize, 10.0f);
	const FNightlightRouteData& Route = Routes[RouteIndex];
	WorldLocations.Reserve(Route.CellsToCore.Num());

	for (const FIntPoint& Coordinate : Route.CellsToCore)
	{
		// Return no route if the saved coordinates no longer match the current grid.
		if (Coordinate.X < 0 || Coordinate.X >= Width || Coordinate.Y < 0 || Coordinate.Y >= Depth)
		{
			WorldLocations.Reset();
			return WorldLocations;
		}

		const int32 CellIndex = GetCellIndex(Coordinate.X, Coordinate.Y, Width);
		if (!Cells.IsValidIndex(CellIndex))
		{
			WorldLocations.Reset();
			return WorldLocations;
		}

		const FVector LocalLocation(
			static_cast<double>(Coordinate.X) * CellSize,
			static_cast<double>(Coordinate.Y) * CellSize,
			Cells[CellIndex].Height);

		// Use the same cell height and generator transform as the terrain and
		// Lucid Anchors (Epic Games, Inc., 2026e).
		WorldLocations.Add(GetActorTransform().TransformPosition(LocalLocation));
	}

	return WorldLocations;
}

TArray<FVector> ANightlightWorldGenerator::GetAnchorWorldLocations() const
{
	TArray<FVector> WorldLocations;
	WorldLocations.Reserve(AnchorCoordinates.Num());

	const int32 Width = FMath::Max(GenerationSettings.GridWidth, 5);
	const int32 Depth = FMath::Max(GenerationSettings.GridDepth, 5);
	const float CellSize = FMath::Max(GenerationSettings.CellSize, 10.0f);

	for (const FIntPoint& Coordinate : AnchorCoordinates)
	{
		// Ignore stale data safely if a designer changes the grid before regenerating.
		if (Coordinate.X < 0 || Coordinate.X >= Width || Coordinate.Y < 0 || Coordinate.Y >= Depth)
		{
			continue;
		}

		const int32 CellIndex = GetCellIndex(Coordinate.X, Coordinate.Y, Width);
		if (!Cells.IsValidIndex(CellIndex))
		{
			continue;
		}

		const FVector LocalLocation(
			static_cast<double>(Coordinate.X) * CellSize,
			static_cast<double>(Coordinate.Y) * CellSize,
			Cells[CellIndex].Height);

		// Anchor data starts in the generator's local grid space. TransformPosition
		// applies the Actor's location, rotation and scale for Blueprint placement
		// (Epic Games, Inc., 2026e).
		WorldLocations.Add(GetActorTransform().TransformPosition(LocalLocation));
	}

	return WorldLocations;
}

void ANightlightWorldGenerator::RebuildTerrainMesh()
{
	// Automation builds logical data on the class default object, which has no
	// gameplay world in which to render or cook collision.
	if (IsTemplate() || !TerrainMesh)
	{
		return;
	}

	FNightlightTerrainMeshData MeshData;
	TerrainMesh->ClearAllMeshSections();
	if (!BuildTerrainMeshData(MeshData))
	{
		UE_LOG(LogTemp, Error, TEXT("Nightlight could not build terrain mesh data from the logical grid."));
		return;
	}

	TArray<FProcMeshTangent> Tangents;
	Tangents.Reserve(MeshData.Normals.Num());
	for (const FVector& Normal : MeshData.Normals)
	{
		// The tangent follows local X while remaining perpendicular to the slope.
		const FVector TangentX = FVector::CrossProduct(FVector::YAxisVector, Normal).GetSafeNormal();
		Tangents.Emplace(TangentX, false);
	}

	TerrainMesh->SetCollisionEnabled(
		bCreateTerrainCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

	// CreateMeshSection consumes the same arrays for rendering and optional
	// collision, so the player cannot collide with a different terrain shape
	// (Epic Games, Inc., 2026d).
	TerrainMesh->CreateMeshSection_LinearColor(
		0,
		MeshData.Vertices,
		MeshData.Triangles,
		MeshData.Normals,
		MeshData.UV0,
		MeshData.VertexColors,
		Tangents,
		bCreateTerrainCollision,
		false);

	if (TerrainMaterial)
	{
		TerrainMesh->SetMaterial(0, TerrainMaterial);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Nightlight rebuilt runtime terrain with %d vertices and %d triangles. Collision: %s."),
		MeshData.Vertices.Num(),
		MeshData.Triangles.Num() / 3,
		bCreateTerrainCollision ? TEXT("enabled") : TEXT("disabled"));
}

bool ANightlightWorldGenerator::BuildTerrainMeshData(FNightlightTerrainMeshData& OutMeshData) const
{
	OutMeshData = FNightlightTerrainMeshData();

	const int32 Width = FMath::Max(GenerationSettings.GridWidth, 5);
	const int32 Depth = FMath::Max(GenerationSettings.GridDepth, 5);
	if (Cells.Num() != Width * Depth)
	{
		return false;
	}

	const float CellSize = FMath::Max(GenerationSettings.CellSize, 10.0f);
	const float UVScale = FMath::Max(GenerationSettings.UVScale, 0.001f);
	const int32 VertexCount = Width * Depth;
	const int32 TriangleIndexCount = (Width - 1) * (Depth - 1) * 6;

	OutMeshData.Vertices.Reserve(VertexCount);
	OutMeshData.Triangles.Reserve(TriangleIndexCount);
	OutMeshData.Normals.SetNumUninitialized(VertexCount);
	OutMeshData.UV0.Reserve(VertexCount);
	OutMeshData.VertexColors.Reserve(VertexCount);

	for (const FNightlightCellData& Cell : Cells)
	{
		// Logical coordinates stay authoritative for both position and cell role.
		OutMeshData.Vertices.Emplace(
			static_cast<double>(Cell.GridCoordinate.X) * CellSize,
			static_cast<double>(Cell.GridCoordinate.Y) * CellSize,
			Cell.Height);
		OutMeshData.UV0.Emplace(
			static_cast<double>(Cell.GridCoordinate.X) * UVScale,
			static_cast<double>(Cell.GridCoordinate.Y) * UVScale);
		OutMeshData.VertexColors.Add(GetCellVertexColor(Cell.Type));
	}

	// Shared vertices keep the surface continuous: each neighbouring sample pair
	// forms two consistently wound triangles (fettis GameDev, 2022).
	for (int32 Y = 0; Y < Depth - 1; ++Y)
	{
		for (int32 X = 0; X < Width - 1; ++X)
		{
			const int32 BottomLeft = GetCellIndex(X, Y, Width);
			const int32 BottomRight = GetCellIndex(X + 1, Y, Width);
			const int32 TopLeft = GetCellIndex(X, Y + 1, Width);
			const int32 TopRight = GetCellIndex(X + 1, Y + 1, Width);

			OutMeshData.Triangles.Append(
				{ BottomLeft, BottomRight, TopLeft, BottomRight, TopRight, TopLeft });
		}
	}

	// Central differences produce smooth normals from the same height samples.
	// Edge samples use their closest available neighbour instead of inventing data.
	for (int32 Y = 0; Y < Depth; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 LeftX = FMath::Max(X - 1, 0);
			const int32 RightX = FMath::Min(X + 1, Width - 1);
			const int32 LowerY = FMath::Max(Y - 1, 0);
			const int32 UpperY = FMath::Min(Y + 1, Depth - 1);

			const FVector TangentX =
				OutMeshData.Vertices[GetCellIndex(RightX, Y, Width)]
				- OutMeshData.Vertices[GetCellIndex(LeftX, Y, Width)];
			const FVector TangentY =
				OutMeshData.Vertices[GetCellIndex(X, UpperY, Width)]
				- OutMeshData.Vertices[GetCellIndex(X, LowerY, Width)];

			OutMeshData.Normals[GetCellIndex(X, Y, Width)] =
				FVector::CrossProduct(TangentX, TangentY).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
		}
	}

	return true;
}

FLinearColor ANightlightWorldGenerator::GetCellVertexColor(const ENightlightCellType CellType)
{
	switch (CellType)
	{
	case ENightlightCellType::Path:
		return FLinearColor(0.20f, 0.45f, 0.85f);
	case ENightlightCellType::Core:
		return FLinearColor(0.85f, 0.75f, 0.20f);
	case ENightlightCellType::Rift:
		return FLinearColor(0.55f, 0.10f, 0.75f);
	case ENightlightCellType::PlacementAnchor:
		return FLinearColor(0.20f, 0.80f, 0.65f);
	case ENightlightCellType::Ground:
	default:
		return FLinearColor(0.08f, 0.22f, 0.10f);
	}
}

void ANightlightWorldGenerator::GenerateRoutes(
	const int32 Width,
	const int32 Depth,
	const FIntPoint& CoreCoordinate,
	FRandomStream& RandomStream)
{
	Routes.Reset();

	// Shuffle the four edge identifiers with the active stream, then sample them
	// without replacement. Each map receives different Rifts while a fixed seed
	// repeats the same selection (Epic Games, Inc., 2026c).
	TArray<int32> EdgeOrder = { 0, 1, 2, 3 };
	for (int32 Index = 0; Index < EdgeOrder.Num() - 1; ++Index)
	{
		const int32 SwapIndex = RandomStream.RandRange(Index, EdgeOrder.Num() - 1);
		EdgeOrder.Swap(Index, SwapIndex);
	}

	const int32 RouteCount = FMath::Clamp(GenerationSettings.RouteCount, 3, 4);
	Routes.Reserve(RouteCount);

	// Each edge owns one final approach cell beside the Core. This prevents two
	// routes from merging early while still giving every route the same target.
	const FIntPoint CoreApproaches[] =
	{
		CoreCoordinate + FIntPoint(0, -1),
		CoreCoordinate + FIntPoint(1, 0),
		CoreCoordinate + FIntPoint(0, 1),
		CoreCoordinate + FIntPoint(-1, 0)
	};

	for (int32 RouteIndex = 0; RouteIndex < RouteCount; ++RouteIndex)
	{
		const int32 EdgeIndex = EdgeOrder[RouteIndex];
		FNightlightRouteData& Route = Routes.AddDefaulted_GetRef();
		Route.RiftCoordinate = SelectRouteEntrance(EdgeIndex, Width, Depth, RandomStream);
		Route.CellsToCore = BuildRouteToCore(
			Route.RiftCoordinate,
			CoreCoordinate,
			CoreApproaches[EdgeIndex],
			RandomStream);
		ApplyRouteToGrid(Route, CoreCoordinate, Width);
	}
}

FIntPoint ANightlightWorldGenerator::SelectRouteEntrance(
	const int32 EdgeIndex,
	const int32 Width,
	const int32 Depth,
	FRandomStream& RandomStream) const
{
	const FIntPoint CoreCoordinate(Width / 2, Depth / 2);

	// Each edge uses a different quadrant. Monotonic routes therefore remain
	// visually separate until they meet at the Core.
	switch (EdgeIndex)
	{
	case 0: // North-west to Core.
		return FIntPoint(RandomStream.RandRange(1, CoreCoordinate.X - 1), 0);
	case 1: // North-east to Core.
		return FIntPoint(Width - 1, RandomStream.RandRange(1, CoreCoordinate.Y - 1));
	case 2: // South-east to Core.
		return FIntPoint(RandomStream.RandRange(CoreCoordinate.X + 1, Width - 2), Depth - 1);
	default: // South-west to Core.
		return FIntPoint(0, RandomStream.RandRange(CoreCoordinate.Y + 1, Depth - 2));
	}
}

TArray<FIntPoint> ANightlightWorldGenerator::BuildRouteToCore(
	const FIntPoint& Start,
	const FIntPoint& CoreCoordinate,
	const FIntPoint& CoreApproach,
	FRandomStream& RandomStream) const
{
	TArray<FIntPoint> RouteCoordinates;
	FIntPoint Current = Start;
	RouteCoordinates.Add(Current);

	// This project-specific route rule reduces Manhattan distance by one cell per
	// step. Seeded weighted choices vary the turns without creating gaps, loops
	// or movement away from the edge's unique Core approach.
	while (Current != CoreApproach)
	{
		const int32 RemainingX = FMath::Abs(CoreApproach.X - Current.X);
		const int32 RemainingY = FMath::Abs(CoreApproach.Y - Current.Y);
		bool bMoveOnX = RemainingY == 0;

		if (RemainingX > 0 && RemainingY > 0)
		{
			bMoveOnX = RandomStream.RandRange(1, RemainingX + RemainingY) <= RemainingX;
		}

		if (bMoveOnX)
		{
			Current.X += CoreApproach.X > Current.X ? 1 : -1;
		}
		else
		{
			Current.Y += CoreApproach.Y > Current.Y ? 1 : -1;
		}

		RouteCoordinates.Add(Current);
	}

	// The final step is unique for each edge, so routes only overlap at the Core.
	RouteCoordinates.Add(CoreCoordinate);
	return RouteCoordinates;
}

void ANightlightWorldGenerator::ApplyRouteToGrid(
	FNightlightRouteData& Route,
	const FIntPoint& CoreCoordinate,
	const int32 Width)
{
	if (Route.CellsToCore.IsEmpty())
	{
		return;
	}

	const float RiftHeight = Cells[GetCellIndex(
		Route.RiftCoordinate.X,
		Route.RiftCoordinate.Y,
		Width)].Height;
	const int32 LastRouteIndex = Route.CellsToCore.Num() - 1;

	for (int32 RouteCellIndex = 0; RouteCellIndex <= LastRouteIndex; ++RouteCellIndex)
	{
		const FIntPoint& Coordinate = Route.CellsToCore[RouteCellIndex];
		FNightlightCellData& Cell = Cells[GetCellIndex(Coordinate.X, Coordinate.Y, Width)];
		const float RouteAlpha = LastRouteIndex > 0
			? static_cast<float>(RouteCellIndex) / static_cast<float>(LastRouteIndex)
			: 1.0f;

		// Blend from the original Rift height to the flat Core. This removes noise
		// spikes and gives future enemies a gradual slope (Epic Games, Inc., 2026a).
		Cell.Height = FMath::Lerp(RiftHeight, 0.0f, RouteAlpha);
		Cell.bBuildable = false;

		if (Coordinate == Route.RiftCoordinate)
		{
			Cell.Type = ENightlightCellType::Rift;
		}
		else if (Coordinate == CoreCoordinate)
		{
			Cell.Type = ENightlightCellType::Core;
		}
		else
		{
			Cell.Type = ENightlightCellType::Path;
		}
	}
}

bool ANightlightWorldGenerator::ValidateGeneratedRoutes(
	const FIntPoint& CoreCoordinate,
	const int32 Width,
	const int32 Depth) const
{
	if (Routes.Num() < 3)
	{
		return false;
	}

	TSet<FIntPoint> UsedRouteCells;
	TSet<int32> RiftEdges;

	for (const FNightlightRouteData& Route : Routes)
	{
		// A complete route starts at its stored Rift and ends at the Core.
		if (Route.CellsToCore.Num() < 2
			|| Route.CellsToCore[0] != Route.RiftCoordinate
			|| Route.CellsToCore.Last() != CoreCoordinate)
		{
			return false;
		}

		const bool bRiftOnEdge = Route.RiftCoordinate.X == 0
			|| Route.RiftCoordinate.X == Width - 1
			|| Route.RiftCoordinate.Y == 0
			|| Route.RiftCoordinate.Y == Depth - 1;
		if (!bRiftOnEdge)
		{
			return false;
		}

		// Corners are excluded during generation, so one identifier can safely
		// represent each of the four map edges.
		const int32 RiftEdge = Route.RiftCoordinate.Y == 0
			? 0
			: (Route.RiftCoordinate.X == Width - 1
				? 1
				: (Route.RiftCoordinate.Y == Depth - 1 ? 2 : 3));
		if (RiftEdges.Contains(RiftEdge))
		{
			return false;
		}
		RiftEdges.Add(RiftEdge);

		for (int32 Index = 0; Index < Route.CellsToCore.Num(); ++Index)
		{
			const FIntPoint& Coordinate = Route.CellsToCore[Index];

			// Reject invalid array access before checking the cell's gameplay role.
			if (Coordinate.X < 0 || Coordinate.X >= Width || Coordinate.Y < 0 || Coordinate.Y >= Depth)
			{
				return false;
			}

			const FNightlightCellData& Cell = Cells[GetCellIndex(Coordinate.X, Coordinate.Y, Width)];
			const ENightlightCellType ExpectedType = Index == 0
				? ENightlightCellType::Rift
				: (Coordinate == CoreCoordinate ? ENightlightCellType::Core : ENightlightCellType::Path);
			if (Cell.Type != ExpectedType || Cell.bBuildable)
			{
				return false;
			}

			if (Index > 0)
			{
				const FIntPoint Step = Coordinate - Route.CellsToCore[Index - 1];

				// A Manhattan distance of one means the next cell is an orthogonal
				// neighbour, preventing diagonal gaps in the stored route.
				if (FMath::Abs(Step.X) + FMath::Abs(Step.Y) != 1)
				{
					return false;
				}
			}

			if (Coordinate != CoreCoordinate)
			{
				// The shared Core is the only legal overlap between separate routes.
				if (UsedRouteCells.Contains(Coordinate))
				{
					return false;
				}
				UsedRouteCells.Add(Coordinate);
			}
		}
	}

	return true;
}

void ANightlightWorldGenerator::GenerateAnchors(
	const int32 Width,
	const int32 Depth,
	FRandomStream& RandomStream)
{
	AnchorCoordinates.Reset();

	const int32 RequestedAnchorCount = FMath::Max(GenerationSettings.AnchorCount, 0);
	if (RequestedAnchorCount == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Nightlight anchors requested: 0. Generated: 0."));
		return;
	}

	TArray<FIntPoint> Candidates;

	// The outside row is kept free for map entrances. Every remaining candidate
	// also checks its neighbours so routes, Rifts and the Core keep one clear cell.
	for (int32 Y = 1; Y < Depth - 1; ++Y)
	{
		for (int32 X = 1; X < Width - 1; ++X)
		{
			const FNightlightCellData& Cell = Cells[GetCellIndex(X, Y, Width)];
			if (Cell.Type != ENightlightCellType::Ground || !Cell.bBuildable)
			{
				continue;
			}

			bool bClearOfReservedCells = true;
			for (int32 OffsetY = -1; OffsetY <= 1 && bClearOfReservedCells; ++OffsetY)
			{
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				{
					const FNightlightCellData& Neighbour =
						Cells[GetCellIndex(X + OffsetX, Y + OffsetY, Width)];
					if (Neighbour.Type == ENightlightCellType::Path
						|| Neighbour.Type == ENightlightCellType::Rift
						|| Neighbour.Type == ENightlightCellType::Core)
					{
						bClearOfReservedCells = false;
						break;
					}
				}
			}

			if (bClearOfReservedCells)
			{
				Candidates.Emplace(X, Y);
			}
		}
	}

	// Shuffle once with the active stream, then accept the first valid positions.
	// This keeps the result simple and repeatable for a fixed seed
	// (Epic Games, Inc., 2026c; Unreal Engine, 2015).
	for (int32 Index = 0; Index < Candidates.Num() - 1; ++Index)
	{
		const int32 SwapIndex = RandomStream.RandRange(Index, Candidates.Num() - 1);
		Candidates.Swap(Index, SwapIndex);
	}

	const int32 MinimumSpacing = FMath::Max(GenerationSettings.MinimumAnchorSpacing, 1);
	const int32 MinimumSpacingSquared = MinimumSpacing * MinimumSpacing;

	for (const FIntPoint& Candidate : Candidates)
	{
		bool bFarEnoughFromOtherAnchors = true;
		for (const FIntPoint& ExistingAnchor : AnchorCoordinates)
		{
			const FIntPoint Difference = Candidate - ExistingAnchor;
			const int32 DistanceSquared = Difference.X * Difference.X + Difference.Y * Difference.Y;
			if (DistanceSquared < MinimumSpacingSquared)
			{
				bFarEnoughFromOtherAnchors = false;
				break;
			}
		}

		if (!bFarEnoughFromOtherAnchors)
		{
			continue;
		}

		AnchorCoordinates.Add(Candidate);
		FNightlightCellData& AnchorCell = Cells[GetCellIndex(Candidate.X, Candidate.Y, Width)];
		AnchorCell.Type = ENightlightCellType::PlacementAnchor;
		AnchorCell.bBuildable = true;

		if (AnchorCoordinates.Num() >= RequestedAnchorCount)
		{
			break;
		}
	}

	if (AnchorCoordinates.Num() < RequestedAnchorCount)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Nightlight anchors requested: %d. Generated: %d. The remaining terrain did not meet clearance and spacing rules."),
			RequestedAnchorCount,
			AnchorCoordinates.Num());
	}
	else
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Nightlight anchors requested: %d. Generated: %d."),
			RequestedAnchorCount,
			AnchorCoordinates.Num());
	}
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

Epic Games, Inc., 2026a. FMath::Lerp. [online] Available at:
<https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FMath/Lerp>
[Accessed 28 August 2026].

Epic Games, Inc., 2026b. FMath::PerlinNoise2D. [online] Available at:
<https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FMath/PerlinNoise2D>
[Accessed 28 August 2026].

Epic Games, Inc., 2026c. Random Streams in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/unreal-engine/random-streams-in-unreal-engine>
[Accessed 28 August 2026].

Epic Games, Inc., 2026d. Create Mesh Section. [online] Available at:
<https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/Components/ProceduralMesh/CreateMeshSection>
[Accessed 30 August 2026].

Epic Games, Inc., 2026e. TTransform::TransformPosition. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/Math/TTransform/TransformPosition>
[Accessed 30 August 2026].

Epic Games, Inc., 2026f. Exposing Gameplay Elements to Blueprints Visual
Scripting in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/unreal-engine/exposing-gameplay-elements-to-blueprints-visual-scripting-in-unreal-engine>
[Accessed 30 August 2026].

fettis GameDev, 2022. Terrain generation in C++ for Beginners - Unreal Engine
tutorial. [video online] Available at: <https://www.youtube.com/watch?v=sNZ2g4qah28>
[Accessed 30 August 2026].

Unreal Engine, 2015. Blueprint Quickshot: Random Streams | 12 | v4.7 Tutorial
Series. [video online] Available at: <https://www.youtube.com/watch?v=kGpsMEMDrjQ>
[Accessed 28 August 2026].
*/
