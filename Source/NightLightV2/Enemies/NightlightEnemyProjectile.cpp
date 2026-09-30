#include "NightlightEnemyProjectile.h"
#include "../Core/NightlightDreamCore.h"
#include "../Defenders/NightlightDefender.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ANightlightEnemyProjectile::ANightlightEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	// Overlap only, so the shot flies over terrain and enemies instead of stopping on them. Overlap events
	// fire only when both components generate them, which the Tick distance check covers
	// (Epic Games, Inc., 2026b).
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(20.0f);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionSphere->SetGenerateOverlapEvents(true);
	SetRootComponent(CollisionSphere);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(CollisionSphere);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The movement component moves the root in a straight line. Gravity is off because the shot is magic,
	// and the starting velocity is along the actor's forward axis, so the enemy spawns it facing the target
	// (Epic Games, Inc., 2026c; Epic Games, Inc., 2026d).
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionSphere);
	ProjectileMovement->InitialSpeed = 1200.0f;
	ProjectileMovement->MaxSpeed = 1200.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
}

void ANightlightEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();

	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ANightlightEnemyProjectile::HandleOverlap);

	// A missed shot removes itself after its lifetime (Epic Games, Inc., 2026a).
	SetLifeSpan(FMath::Max(Lifetime, 0.1f));
}

void ANightlightEnemyProjectile::Launch(AActor* const InTarget, const float InDamage)
{
	Target = InTarget;
	Damage = FMath::Max(InDamage, 0.0f);
}

void ANightlightEnemyProjectile::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bHasHit || !IsValid(Target))
	{
		return;
	}

	// Defenders and the Core do not move, so reaching the target's position counts as a hit.
	if (FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(FMath::Max(HitRadius, 0.0f)))
	{
		TryHit(Target);
	}
}

void ANightlightEnemyProjectile::HandleOverlap(
	UPrimitiveComponent* const OverlappedComponent,
	AActor* const OtherActor,
	UPrimitiveComponent* const OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	// Any defender or the Core in the way takes the hit. Enemies, including the shooter, are ignored.
	TryHit(OtherActor);
}

bool ANightlightEnemyProjectile::TryHit(AActor* const HitActor)
{
	if (bHasHit || !IsValid(HitActor))
	{
		return false;
	}

	if (ANightlightDefender* const Defender = Cast<ANightlightDefender>(HitActor))
	{
		if (Defender->IsDead())
		{
			return false;
		}

		Defender->ApplyDamage(Damage);
	}
	else if (ANightlightDreamCore* const DreamCore = Cast<ANightlightDreamCore>(HitActor))
	{
		if (DreamCore->IsCoreDestroyed())
		{
			return false;
		}

		DreamCore->ApplyCoreDamage(Damage);
	}
	else
	{
		return false;
	}

	// Set this first so an overlap in the same frame cannot damage a second actor.
	bHasHit = true;
	OnProjectileImpact(HitActor);
	Destroy();
	return true;
}

/*
References

Epic Games, Inc., 2026a. AActor::SetLifeSpan. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AActor/SetLifeSpan>
[Accessed 30 September 2026].

Epic Games, Inc., 2026b. Collision in Unreal Engine - Overview. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/collision-in-unreal-engine---overview>
[Accessed 30 September 2026].

Epic Games, Inc., 2026c. Implementing Projectiles in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/implementing-projectiles-in-unreal-engine>
[Accessed 30 September 2026].

Epic Games, Inc., 2026d. UProjectileMovementComponent. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UProjectileMovementComponent>
[Accessed 30 September 2026].
*/
