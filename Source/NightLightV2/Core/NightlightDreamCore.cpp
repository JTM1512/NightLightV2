#include "NightlightDreamCore.h"
#include "../Enemies/NightlightEnemy.h"
#include "../Systems/NightlightActorRegistrySubsystem.h"
#include "Components/LightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ANightlightDreamCore::ANightlightDreamCore()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(SceneRoot);

	// Each tier adds health and defender slots, and grows the Core with a new light colour so the
	// upgrade is easy to see from the camera.
	FNightlightUpgradeTier LucidTier;
	LucidTier.DisplayName = NSLOCTEXT("Nightlight", "CoreTierLucid", "Lucid Core");
	LucidTier.Cost = 150;
	LucidTier.MaxHealthBonus = 50.0f;
	LucidTier.ExtraDefenderSlots = 3;
	LucidTier.VisualScale = 1.15f;
	LucidTier.TintColor = FLinearColor(0.35f, 0.65f, 1.0f);
	LucidTier.GlowIntensityScale = 1.6f;
	UpgradeTiers.Add(LucidTier);

	FNightlightUpgradeTier RadiantTier;
	RadiantTier.DisplayName = NSLOCTEXT("Nightlight", "CoreTierRadiant", "Radiant Core");
	RadiantTier.Cost = 300;
	RadiantTier.MaxHealthBonus = 75.0f;
	RadiantTier.ExtraDefenderSlots = 3;
	RadiantTier.VisualScale = 1.3f;
	RadiantTier.TintColor = FLinearColor(1.0f, 0.75f, 0.3f);
	RadiantTier.GlowIntensityScale = 2.4f;
	UpgradeTiers.Add(RadiantTier);
}

void ANightlightDreamCore::BeginPlay()
{
	Super::BeginPlay();

	// Blueprint children can change MaxHealth, so copy it when the game starts.
	MaxHealth = FMath::Max(MaxHealth, 0.0f);
	CurrentHealth = MaxHealth;
	bCoreDestroyed = false;
	UpgradeLevel = 0;
	SaveVisualBase();
	OnCoreHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		bCoreDestroyed = true;
		OnCoreDestroyed.Broadcast();
		return;
	}

	// The Core attacks on a timer so it does not search for enemies every frame
	// (Epic Games, Inc., 2026a).
	GetWorldTimerManager().SetTimer(
		AttackTimerHandle,
		this,
		&ANightlightDreamCore::AttackNearestEnemy,
		FMath::Max(AttackInterval, 0.1f),
		true);
}

void ANightlightDreamCore::ApplyCoreDamage(const float DamageAmount)
{
	if (bCoreDestroyed || DamageAmount <= 0.0f)
	{
		return;
	}

	// A large hit can reach zero, but it must never leave the Core with negative health.
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	OnCoreHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		// Set this first because an event listener could try to damage the Core again.
		bCoreDestroyed = true;
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		OnCoreDestroyed.Broadcast();
	}
}

bool ANightlightDreamCore::UpgradeCore()
{
	if (!CanUpgradeCore())
	{
		return false;
	}

	const FNightlightUpgradeTier& Tier = GetUpgradeTiers()[UpgradeLevel];
	++UpgradeLevel;

	// Current health rises with the max, so an upgrade also repairs the Core by the bonus.
	const float HealthBonus = FMath::Max(Tier.MaxHealthBonus, 0.0f);
	MaxHealth += HealthBonus;
	CurrentHealth = FMath::Clamp(CurrentHealth + HealthBonus, 0.0f, MaxHealth);
	AttackDamage += FMath::Max(Tier.AttackDamageBonus, 0.0f);

	ApplyTierVisuals(Tier);

	OnCoreHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	OnCoreUpgraded.Broadcast(UpgradeLevel, GetMaxUpgradeLevel());
	return true;
}

bool ANightlightDreamCore::CanUpgradeCore() const
{
	return !bCoreDestroyed && UpgradeLevel < GetMaxUpgradeLevel();
}

bool ANightlightDreamCore::GetNextUpgradeTier(FNightlightUpgradeTier& OutTier) const
{
	const TArray<FNightlightUpgradeTier>& Tiers = GetUpgradeTiers();
	if (!Tiers.IsValidIndex(UpgradeLevel))
	{
		return false;
	}

	OutTier = Tiers[UpgradeLevel];
	return true;
}

int32 ANightlightDreamCore::GetNextUpgradeCost() const
{
	const TArray<FNightlightUpgradeTier>& Tiers = GetUpgradeTiers();
	return Tiers.IsValidIndex(UpgradeLevel) ? FMath::Max(Tiers[UpgradeLevel].Cost, 0) : -1;
}

int32 ANightlightDreamCore::GetMaxDefenders() const
{
	const TArray<FNightlightUpgradeTier>& Tiers = GetUpgradeTiers();
	int32 MaxDefenders = FMath::Max(BaseMaxDefenders, 0);
	for (int32 TierIndex = 0; TierIndex < UpgradeLevel && Tiers.IsValidIndex(TierIndex); ++TierIndex)
	{
		MaxDefenders += FMath::Max(Tiers[TierIndex].ExtraDefenderSlots, 0);
	}
	return MaxDefenders;
}

int32 ANightlightDreamCore::GetDefenderCount() const
{
	const UNightlightActorRegistrySubsystem* const Registry = GetWorld() ? GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>() : nullptr;
	return Registry ? Registry->GetDefenders().Num() : 0;
}

const TArray<FNightlightUpgradeTier>& ANightlightDreamCore::GetUpgradeTiers() const
{
	return UpgradeData ? UpgradeData->Tiers : UpgradeTiers;
}

ULightComponent* ANightlightDreamCore::FindCoreLight() const
{
	// BP_DreamCore adds its own light, so it is found by type instead of being a C++ component.
	return FindComponentByClass<ULightComponent>();
}

void ANightlightDreamCore::SaveVisualBase()
{
	BaseMeshScale = CoreMesh ? CoreMesh->GetRelativeScale3D() : FVector::OneVector;
	const ULightComponent* const CoreLight = FindCoreLight();
	BaseLightIntensity = CoreLight ? CoreLight->Intensity : 0.0f;
	bVisualBaseSaved = true;
}

void ANightlightDreamCore::ApplyTierVisuals(const FNightlightUpgradeTier& Tier)
{
	if (!bVisualBaseSaved)
	{
		SaveVisualBase();
	}

	if (CoreMesh)
	{
		// A static component cannot be changed during play, so the Core switches to movable first.
		CoreMesh->SetMobility(EComponentMobility::Movable);

		if (Tier.MeshOverride)
		{
			CoreMesh->SetStaticMesh(Tier.MeshOverride);
		}

		if (Tier.MaterialOverride)
		{
			for (int32 MaterialIndex = 0; MaterialIndex < CoreMesh->GetNumMaterials(); ++MaterialIndex)
			{
				CoreMesh->SetMaterial(MaterialIndex, Tier.MaterialOverride);
			}
		}

		// Scaled from the starting size, not the last tier's, so tiers do not multiply together.
		CoreMesh->SetRelativeScale3D(BaseMeshScale * FMath::Max(Tier.VisualScale, 0.1f));
	}

	if (ULightComponent* const CoreLight = FindCoreLight())
	{
		CoreLight->SetMobility(EComponentMobility::Movable);
		CoreLight->SetLightColor(Tier.TintColor);
		CoreLight->SetIntensity(BaseLightIntensity * FMath::Max(Tier.GlowIntensityScale, 0.0f));
	}
}

void ANightlightDreamCore::AttackNearestEnemy()
{
	if (bCoreDestroyed || AttackRange <= 0.0f || AttackDamage <= 0.0f)
	{
		return;
	}

	// Every enemy registers itself, so the registry replaces a GetAllActorsOfClass search, which is
	// slow when there are many actors (Epic Games, Inc., 2026b). Dead enemies are skipped.
	const UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>();
	ANightlightEnemy* const ClosestEnemy = Registry ? Registry->FindClosestEnemy(GetActorLocation(), AttackRange) : nullptr;
	if (ClosestEnemy)
	{
		ClosestEnemy->ApplyDamage(AttackDamage);
	}
}

/*
References

Epic Games, Inc., 2026a. Gameplay Timers in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-timers-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026b. UGameplayStatics::GetAllActorsOfClass. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameplayStatics/GetAllActorsOfClass>
[Accessed 29 September 2026].
*/
