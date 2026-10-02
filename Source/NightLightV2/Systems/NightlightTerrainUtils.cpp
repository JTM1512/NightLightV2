#include "NightlightTerrainUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace NightlightTerrainUtils
{
	void PlaceOnTerrain(AActor* const Actor, const float Yaw, const float HoverHeight)
	{
		if (!IsValid(Actor) || !Actor->GetWorld())
		{
			return;
		}

		// Units, crates and the Core are all WorldDynamic, so a WorldStatic query only finds the
		// terrain and the ground plane around it (Epic Games, Inc., 2026b).
		const FRotator Upright(0.0f, Yaw, 0.0f);
		const FVector Location = Actor->GetActorLocation();
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NightlightTerrain), false, Actor);
		FHitResult Hit;
		if (!Actor->GetWorld()->LineTraceSingleByObjectType(
			Hit,
			Location + FVector(0.0, 0.0, 300.0),
			Location - FVector(0.0, 0.0, 500.0 + HoverHeight),
			FCollisionObjectQueryParams(ECC_WorldStatic),
			QueryParams))
		{
			Actor->SetActorRotation(Upright);
			return;
		}

		if (HoverHeight > 0.0f)
		{
			Actor->SetActorLocationAndRotation(Hit.ImpactPoint + FVector(0.0, 0.0, HoverHeight), Upright);
			return;
		}

		// MakeFromZX keeps the surface normal as up and the facing as close to forward as the slope
		// allows, so the unit lies flat on the slope instead of sinking into it (Epic Games, Inc., 2026a).
		Actor->SetActorLocationAndRotation(
			Hit.ImpactPoint,
			FRotationMatrix::MakeFromZX(Hit.ImpactNormal, Upright.Vector()).Rotator());
	}
}

/*
References

Epic Games, Inc., 2026a. FRotationMatrix::MakeFromZX. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FRotationMatrix/MakeFromZX>
[Accessed 2 October 2026].

Epic Games, Inc., 2026b. UWorld::LineTraceSingleByObjectType. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorld/LineTraceSingleByObjectType>
[Accessed 2 October 2026].
*/
