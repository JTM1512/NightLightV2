#pragma once

#include "CoreMinimal.h"

class AActor;

// Enemies and moving defenders share this so every unit stands on the terrain the same way.
namespace NightlightTerrainUtils
{
	// Stands the actor on the terrain under it and tilts it with the slope, keeping the given yaw.
	// A hover height above zero keeps it level at that height instead, so it flies over the slope.
	// Where there is no terrain, such as in a test world, only the yaw is applied.
	void PlaceOnTerrain(AActor* Actor, float Yaw, float HoverHeight = 0.0f);
}
