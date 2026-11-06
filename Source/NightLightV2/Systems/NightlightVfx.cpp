#include "NightlightVfx.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	// Sparks thrown by a burst of intensity 1. The count is set from code because a Niagara spawn
	// count is a whole number and the intensity is not.
	constexpr float SparksPerIntensity = 30.0f;
	constexpr int32 MinSparks = 4;
	constexpr int32 MaxSparks = 200;
}

namespace NightlightVfx
{
	void SpawnBurst(
		UNiagaraSystem* const System,
		const AActor* const Target,
		const FLinearColor& Tint,
		const float Intensity,
		const float Radius)
	{
		if (!System || !Target)
		{
			return;
		}

		// The Core mesh's pivot sits on one corner, so the burst is placed from the meshes' bounds
		// instead of the actor's location (Epic Games, Inc., 2026a).
		const FBox Bounds = Target->GetComponentsBoundingBox();
		const FVector Location = Bounds.IsValid
			? FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z)
			: Target->GetActorLocation();

		SpawnBurstAtLocation(System, Target, Location, Tint, Intensity, Radius);
	}

	void SpawnBurstAtLocation(
		UNiagaraSystem* const System,
		const UObject* const WorldContext,
		const FVector& Location,
		const FLinearColor& Tint,
		const float Intensity,
		const float Radius)
	{
		if (!System || !WorldContext)
		{
			return;
		}

		// Pooled components are reused instead of created for every burst, which matters when many
		// enemies are hit at once. The component starts inactive so the values below are in place
		// before the first particles spawn (Epic Games, Inc., 2026c).
		UNiagaraComponent* const Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			WorldContext,
			System,
			Location,
			FRotator::ZeroRotator,
			FVector::OneVector,
			true,
			false,
			ENCPoolMethod::AutoRelease);
		if (!Burst)
		{
			return;
		}

		// A pooled component keeps the values of its last burst, so all four are set every time
		// (Epic Games, Inc., 2026b).
		const float SafeIntensity = FMath::Max(Intensity, 0.0f);
		const int32 SparkCount = FMath::Clamp(FMath::RoundToInt(SparksPerIntensity * SafeIntensity), MinSparks, MaxSparks);
		Burst->SetVariableLinearColor(TEXT("User.TintColor"), Tint);
		Burst->SetVariableFloat(TEXT("User.Intensity"), SafeIntensity);
		Burst->SetVariableFloat(TEXT("User.Radius"), FMath::Max(Radius, 0.0f));
		Burst->SetVariableInt(TEXT("User.SparkCount"), SparkCount);
		Burst->Activate(true);
	}
}

/*
References

Epic Games, Inc., 2026a. AActor::GetComponentsBoundingBox. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AActor/GetComponentsBoundingBox>
[Accessed 5 November 2026].

Epic Games, Inc., 2026b. UNiagaraComponent. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/Niagara/UNiagaraComponent>
[Accessed 5 November 2026].

Epic Games, Inc., 2026c. UNiagaraFunctionLibrary::SpawnSystemAtLocation. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/Niagara/UNiagaraFunctionLibrary/SpawnSystemAtLocation>
[Accessed 5 November 2026].
*/
