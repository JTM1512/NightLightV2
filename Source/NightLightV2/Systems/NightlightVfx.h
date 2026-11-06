#pragma once

#include "CoreMinimal.h"

class AActor;
class UNiagaraSystem;
class UObject;

// The Core, defenders and enemies spawn their particle effects through this, so every effect gets its
// colour, size and strength the same way.
namespace NightlightVfx
{
	// Plays a one-shot Niagara system at the bottom middle of the actor's meshes. The system reads
	// User.TintColor, User.Intensity, User.Radius and User.SparkCount. Intensity 1 gives the normal
	// burst, 2 a burst twice as strong. Nothing plays when no system is set, so a Blueprint without one
	// still works.
	void SpawnBurst(UNiagaraSystem* System, const AActor* Target, const FLinearColor& Tint, float Intensity, float Radius);

	// The same, at a given point, such as the middle of an enemy that was hit.
	void SpawnBurstAtLocation(
		UNiagaraSystem* System,
		const UObject* WorldContext,
		const FVector& Location,
		const FLinearColor& Tint,
		float Intensity,
		float Radius);
}
