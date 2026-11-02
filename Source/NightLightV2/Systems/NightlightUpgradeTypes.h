#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NightlightUpgradeTypes.generated.h"

class UMaterialInterface;
class UStaticMesh;

// One upgrade step. The Core and the defenders share this format, so the HUD can show and buy any
// upgrade the same way. A field that does not apply to a target is left at its default and ignored.
USTRUCT(BlueprintType)
struct FNightlightUpgradeTier
{
	GENERATED_BODY()

	// Shown on the HUD upgrade button.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Upgrade")
	FText DisplayName;

	// Dream Tokens the HUD checks with CanAfford and spends before it applies the tier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Upgrade", meta = (ClampMin = "0"))
	int32 Cost = 100;

	// Added to max health. Current health rises by the same amount, so the damage already taken is kept.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Upgrade|Stats", meta = (ClampMin = "0.0"))
	float MaxHealthBonus = 0.0f;

	// Added to the damage of each attack.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Upgrade|Stats", meta = (ClampMin = "0.0"))
	float AttackDamageBonus = 0.0f;

	// Core only: extra defenders the player may have in play at once.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Upgrade|Stats", meta = (ClampMin = "0"))
	int32 ExtraDefenderSlots = 0;

	// Multiplies the mesh's starting scale, so each tier looks bigger than the last.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Upgrade|Visuals", meta = (ClampMin = "0.1"))
	float VisualScale = 1.0f;

	// Colour of the target's light once this tier is bought.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Upgrade|Visuals")
	FLinearColor TintColor = FLinearColor::White;

	// Multiplies the light's starting intensity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Upgrade|Visuals", meta = (ClampMin = "0.0"))
	float GlowIntensityScale = 1.0f;

	// Optional. Replaces the mesh when set, for example with a larger tower model.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Upgrade|Visuals")
	TObjectPtr<UStaticMesh> MeshOverride;

	// Optional. Replaces every material on the mesh when set.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Upgrade|Visuals")
	TObjectPtr<UMaterialInterface> MaterialOverride;
};

// A list of tiers for one upgradeable thing, made as a Data Asset in the editor so the tiers can be
// tuned without code (Epic Games, Inc., 2026).
UCLASS(BlueprintType)
class NIGHTLIGHTV2_API UNightlightUpgradeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Bought in order. Index 0 is the first upgrade, not the starting state.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightlight|Upgrade")
	TArray<FNightlightUpgradeTier> Tiers;
};

/*
References

Epic Games, Inc., 2026. Data Assets in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/data-assets-in-unreal-engine>
[Accessed 2 November 2026].
*/
