#include "NightlightCameraHeightComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UNightlightCameraHeightComponent::UNightlightCameraHeightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UNightlightCameraHeightComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* const Pawn = Cast<APawn>(GetOwner());
	const APlayerController* const Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Controller)
	{
		return;
	}

	// Reading the keys straight from the player controller keeps this separate from the
	// controller's own input mappings (Epic Games, Inc., 2026).
	const float HeldDirection =
		static_cast<float>(Controller->IsInputKeyDown(EKeys::E)) - static_cast<float>(Controller->IsInputKeyDown(EKeys::Q));
	const float WheelDirection =
		static_cast<float>(Controller->WasInputKeyJustPressed(EKeys::MouseScrollUp))
		- static_cast<float>(Controller->WasInputKeyJustPressed(EKeys::MouseScrollDown));
	const float HeightChange = HeldDirection * HeightSpeed * DeltaTime + WheelDirection * WheelStep;
	if (FMath::IsNearlyZero(HeightChange))
	{
		return;
	}

	FVector Location = GetOwner()->GetActorLocation();
	Location.Z = FMath::Clamp(Location.Z + HeightChange, MinHeight, MaxHeight);
	GetOwner()->SetActorLocation(Location);
}

/*
References

Epic Games, Inc., 2026. APlayerController. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/APlayerController>
[Accessed 2 October 2026].
*/
