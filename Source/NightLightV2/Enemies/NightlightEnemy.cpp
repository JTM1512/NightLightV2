#include "NightlightEnemy.h"
#include "../Core/NightlightDreamCore.h"
#include "../Defenders/NightlightDefender.h"
#include "../Systems/NightlightActorRegistrySubsystem.h"
#include "../Systems/NightlightTerrainUtils.h"
#include "../UI/NightlightHealthWidgetUtils.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

ANightlightEnemy::ANightlightEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANightlightEnemy::BeginPlay()
{
	Super::BeginPlay();

	// The registry lets defenders, the Core and the later wave director find this enemy without an actor search.
	if (UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>())
	{
		Registry->RegisterEnemy(this);
	}

	// Blueprint enemy types can change MaxHealth, so copy it when the game starts.
	MaxHealth = FMath::Max(MaxHealth, 0.0f);
	CurrentHealth = MaxHealth;
	bIsDead = false;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	NightlightHealthWidgetUtils::UpdateWorldHealthWidget(this, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void ANightlightEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EndPlay runs when the enemy dies, reaches the Core or the level ends, so this one call covers
	// every way an enemy leaves play (Epic Games, Inc., 2026i). A dead enemy has already left the
	// registry in Die, and a second removal is ignored.
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitFlashTimerHandle);
		if (UNightlightActorRegistrySubsystem* const Registry = World->GetSubsystem<UNightlightActorRegistrySubsystem>())
		{
			Registry->UnregisterEnemy(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ANightlightEnemy::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!UpdateDefenderCombat(DeltaTime) || !ShouldStopForDefender())
	{
		MoveAlongRoute(DeltaTime);
	}

	FaceCoreOnTerrain(DeltaTime);
}

float ANightlightEnemy::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* const EventInstigator,
	AActor* const DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	ApplyDamage(AppliedDamage);
	return AppliedDamage;
}

void ANightlightEnemy::AssignRoute(const TArray<FVector>& RoutePoints)
{
	// Raising the copied points by the hover height lets the enemy still reach each point while it floats.
	AssignedRoutePoints = RoutePoints;
	for (FVector& RoutePoint : AssignedRoutePoints)
	{
		RoutePoint.Z += HoverHeight;
	}

	CurrentWaypointIndex = INDEX_NONE;
	bHasReachedCore = false;
	SetActorTickEnabled(false);
	if (bIsDead)
	{
		return;
	}

	if (AssignedRoutePoints.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight enemy received an incomplete route and will remain stopped."));
		return;
	}

	// The first point is the Rift spawn position, so movement starts at point one.
	SetActorLocation(AssignedRoutePoints[0]);
	FaceCoreOnTerrain(0.0f);
	CurrentWaypointIndex = 1;
	SetActorTickEnabled(true);
}

void ANightlightEnemy::AssignDreamCore(ANightlightDreamCore* const InDreamCore)
{
	DreamCore = InDreamCore;
	if (IsValid(DreamCore))
	{
		// Only colliding parts count, so the Core's health bar does not pull the point upwards
		// (Epic Games, Inc., 2026a).
		CoreFacingLocation = DreamCore->GetComponentsBoundingBox().GetCenter();
	}
}

void ANightlightEnemy::FaceCoreOnTerrain(const float DeltaTime)
{
	// Movement can end at the Core, which destroys the enemy, and a dying enemy stays where it fell.
	if (bIsDead || bHasReachedCore)
	{
		return;
	}

	// A new route snaps straight to the Core, then RInterpTo eases the turn every Tick
	// (Epic Games, Inc., 2026c).
	float Yaw = GetActorRotation().Yaw;
	if (IsValid(DreamCore))
	{
		const FRotator Facing(0.0f, (CoreFacingLocation - GetActorLocation()).Rotation().Yaw, 0.0f);
		Yaw = DeltaTime > 0.0f ? FMath::RInterpTo(FRotator(0.0f, Yaw, 0.0f), Facing, DeltaTime, TurnSpeed).Yaw : Facing.Yaw;
	}

	// Route points only sit on the terrain at each cell, so the enemy is placed over the slope between them.
	NightlightTerrainUtils::PlaceOnTerrain(this, Yaw, HoverHeight);
}

void ANightlightEnemy::ApplyDamage(const float DamageAmount)
{
	if (bIsDead || DamageAmount <= 0.0f)
	{
		return;
	}

	// A strong attack can reach zero, but health must never become negative.
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	OnDamageTaken.Broadcast(AppliedDamage);
	NightlightHealthWidgetUtils::UpdateWorldHealthWidget(this, CurrentHealth, MaxHealth, AppliedDamage);

	// The lethal hit flashes too, so the player sees the blow that killed the enemy.
	StartHitFlash();

	if (CurrentHealth > 0.0f)
	{
		return;
	}

	Die();
}

void ANightlightEnemy::MoveAlongRoute(const float DeltaTime)
{
	if (bIsDead || bHasReachedCore || !AssignedRoutePoints.IsValidIndex(CurrentWaypointIndex))
	{
		SetActorTickEnabled(false);
		return;
	}

	const FVector TargetLocation = AssignedRoutePoints[CurrentWaypointIndex];
	const FVector NewLocation = FMath::VInterpConstantTo(
		GetActorLocation(),
		TargetLocation,
		DeltaTime,
		FMath::Max(MovementSpeed, 0.0f));

	// VInterpConstantTo stops at the target instead of moving past the waypoint
	// (Epic Games, Inc., 2026c).
	SetActorLocation(NewLocation);

	const float AcceptanceDistance = FMath::Max(WaypointAcceptanceDistance, 0.0f);
	if (FVector::DistSquared(NewLocation, TargetLocation) <= FMath::Square(AcceptanceDistance))
	{
		SetActorLocation(TargetLocation);
		ReachNextWaypoint();
	}
}

bool ANightlightEnemy::UpdateDefenderCombat(const float DeltaTime)
{
	if (bIsDead || bHasReachedCore)
	{
		ClearDefenderTarget();
		return false;
	}

	if (IsValid(TargetDefender) && TargetDefender->IsDead())
	{
		ClearDefenderTarget();
	}

	const float AttackRangeSquared = FMath::Square(FMath::Max(DefenderAttackRange, 0.0f));
	if (IsValid(TargetDefender))
	{
		if (FVector::DistSquared(GetActorLocation(), TargetDefender->GetActorLocation()) <= AttackRangeSquared)
		{
			return true;
		}

		ClearDefenderTarget();
	}

	TimeUntilDefenderSearch -= DeltaTime;
	if (TimeUntilDefenderSearch > 0.0f)
	{
		return false;
	}

	// A short search delay keeps this simple without scanning every defender every frame.
	TimeUntilDefenderSearch = 0.25f;
	TargetDefender = FindDefenderTarget();
	if (!IsValid(TargetDefender))
	{
		TargetDefender = nullptr;
		return false;
	}

	// Damage starts immediately, then repeats while the same defender remains in range
	// (Epic Games, Inc., 2026d).
	AttackTargetDefender();
	if (!IsValid(TargetDefender))
	{
		return false;
	}

	GetWorldTimerManager().SetTimer(
		DefenderAttackTimerHandle,
		this,
		&ANightlightEnemy::AttackTargetDefender,
		FMath::Max(DefenderAttackInterval, 0.1f),
		true);
	return true;
}

ANightlightDefender* ANightlightEnemy::FindDefenderTarget()
{
	// Every defender type registers itself, so the registry replaces a GetAllActorsOfClass search,
	// which is slow when there are many actors (Epic Games, Inc., 2026g). Dead defenders are skipped.
	const UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>();
	return Registry ? Registry->FindClosestDefender(GetActorLocation(), DefenderAttackRange) : nullptr;
}

void ANightlightEnemy::AttackTargetDefender()
{
	if (bIsDead || bHasReachedCore || !IsValid(TargetDefender))
	{
		ClearDefenderTarget();
		return;
	}

	const float AttackRangeSquared = FMath::Square(FMath::Max(DefenderAttackRange, 0.0f));
	if (FVector::DistSquared(GetActorLocation(), TargetDefender->GetActorLocation()) > AttackRangeSquared)
	{
		ClearDefenderTarget();
		return;
	}

	// The event fires before the hit so the Blueprint still has a valid defender if this hit
	// destroys it. Blueprints implement it without any C++ body (Epic Games, Inc., 2026f).
	OnAttackDefender(TargetDefender);
	AttackDefender(TargetDefender);
	if (!IsValid(TargetDefender) || TargetDefender->IsDead())
	{
		ClearDefenderTarget();
	}
}

void ANightlightEnemy::AttackDefender(ANightlightDefender* const Defender)
{
	if (IsValid(Defender))
	{
		Defender->ApplyDamage(FMath::Max(DefenderAttackDamage, 0.0f));
	}
}

bool ANightlightEnemy::ShouldStopForDefender() const
{
	return true;
}

void ANightlightEnemy::ClearDefenderTarget()
{
	GetWorldTimerManager().ClearTimer(DefenderAttackTimerHandle);
	TargetDefender = nullptr;
}

void ANightlightEnemy::ReachNextWaypoint()
{
	++CurrentWaypointIndex;
	if (CurrentWaypointIndex >= AssignedRoutePoints.Num())
	{
		HandleCoreReached();
	}
}

void ANightlightEnemy::HandleCoreReached()
{
	if (bIsDead || bHasReachedCore)
	{
		return;
	}

	// Set this before the damage and event so another Tick cannot repeat the arrival.
	bHasReachedCore = true;
	SetActorTickEnabled(false);
	ClearDefenderTarget();

	if (IsValid(DreamCore) && !DreamCore->IsCoreDestroyed())
	{
		DreamCore->ApplyCoreDamage(FMath::Max(CoreDamage, 0.0f));
	}

	OnCoreReached();
	Destroy();
}

void ANightlightEnemy::Die()
{
	if (bIsDead)
	{
		return;
	}

	// Stop movement before the event because a dead enemy must never reach the Core.
	bIsDead = true;
	SetActorTickEnabled(false);
	ClearDefenderTarget();

	// Leave the registry now instead of in EndPlay, so no defender or Core picks this enemy as a
	// target while its death effect plays.
	if (UNightlightActorRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UNightlightActorRegistrySubsystem>())
	{
		Registry->UnregisterEnemy(this);
	}

	AwardDeathTokens(TokensOnDeath);
	OnEnemyDied.Broadcast();

	// Blueprints spawn their burst or sound here, while the enemy is still in the level.
	OnDeathEffects();
	if (DeathRemovalDelay <= 0.0f)
	{
		Destroy();
		return;
	}

	// Without collision the dying enemy cannot block or catch shots. The life span destroys it once
	// the delay ends (Epic Games, Inc., 2026b).
	SetActorEnableCollision(false);
	SetLifeSpan(DeathRemovalDelay);
}

void ANightlightEnemy::CreateHitFlashMaterials()
{
	bHasCreatedHitFlashMaterials = true;

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* const Mesh : Meshes)
	{
		// The health bar is a widget component, which is also a mesh component, but it manages its own material.
		if (!IsValid(Mesh) || Mesh->IsA<UWidgetComponent>())
		{
			continue;
		}

		for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
		{
			// Only materials with the parameter get a dynamic instance, so the others keep batching together.
			UMaterialInterface* const Material = Mesh->GetMaterial(MaterialIndex);
			float StartingFlash = 0.0f;
			if (!Material || !Material->GetScalarParameterValue(FHashedMaterialParameterInfo(HitFlashParameterName), StartingFlash))
			{
				continue;
			}

			// A parameter can only be changed while playing on a dynamic instance of the material
			// (Epic Games, Inc., 2026e; Epic Games, Inc., 2026j).
			if (UMaterialInstanceDynamic* const DynamicMaterial = Mesh->CreateDynamicMaterialInstance(MaterialIndex, Material))
			{
				HitFlashMaterials.Add(DynamicMaterial);
			}
		}
	}
}

void ANightlightEnemy::StartHitFlash()
{
	if (HitFlashDuration <= 0.0f || HitFlashParameterName.IsNone())
	{
		return;
	}

	if (!bHasCreatedHitFlashMaterials)
	{
		CreateHitFlashMaterials();
	}

	if (HitFlashMaterials.IsEmpty())
	{
		return;
	}

	// A short flash on impact makes each hit readable, even in a crowd (Swink, 2007).
	SetHitFlashAmount(1.0f);
	HitFlashEndTime = GetWorld()->GetTimeSeconds() + HitFlashDuration;

	// A fast repeating timer fades the flash out. A new hit simply restarts it (Epic Games, Inc., 2026d).
	GetWorldTimerManager().SetTimer(
		HitFlashTimerHandle,
		this,
		&ANightlightEnemy::UpdateHitFlash,
		0.02f,
		true);
}

void ANightlightEnemy::UpdateHitFlash()
{
	const float TimeRemaining = HitFlashEndTime - GetWorld()->GetTimeSeconds();
	if (TimeRemaining <= 0.0f)
	{
		SetHitFlashAmount(0.0f);
		GetWorldTimerManager().ClearTimer(HitFlashTimerHandle);
		return;
	}

	SetHitFlashAmount(TimeRemaining / HitFlashDuration);
}

void ANightlightEnemy::SetHitFlashAmount(const float Amount)
{
	for (UMaterialInstanceDynamic* const DynamicMaterial : HitFlashMaterials)
	{
		if (IsValid(DynamicMaterial))
		{
			// The dynamic instance takes the new value straight away (Epic Games, Inc., 2026h).
			DynamicMaterial->SetScalarParameterValue(HitFlashParameterName, Amount);
		}
	}
}

/*
References

Epic Games, Inc., 2026a. AActor::GetComponentsBoundingBox. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AActor/GetComponentsBoundingBox>
[Accessed 2 October 2026].

Epic Games, Inc., 2026b. AActor::SetLifeSpan. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AActor/SetLifeSpan>
[Accessed 30 September 2026].

Epic Games, Inc., 2026c. FMath. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FMath>
[Accessed 2 October 2026].

Epic Games, Inc., 2026d. Gameplay Timers in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-timers-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026e. Instanced Materials in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-materials-in-unreal-engine>
[Accessed 30 September 2026].

Epic Games, Inc., 2026f. UFunctions in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/ufunctions-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026g. UGameplayStatics::GetAllActorsOfClass. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameplayStatics/GetAllActorsOfClass>
[Accessed 29 September 2026].

Epic Games, Inc., 2026h. UMaterialInstanceDynamic. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UMaterialInstanceDynamic>
[Accessed 30 September 2026].

Epic Games, Inc., 2026i. Unreal Engine Actor Lifecycle. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-actor-lifecycle>
[Accessed 29 September 2026].

Epic Games, Inc., 2026j. UPrimitiveComponent. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UPrimitiveComponent>
[Accessed 30 September 2026].

Swink, S., 2007. Game Feel: The Secret Ingredient. [online] Available at:
<https://www.gamedeveloper.com/design/game-feel-the-secret-ingredient> [Accessed 30 September 2026].
*/
