#include "NightlightActorRegistrySubsystem.h"
#include "../Defenders/NightlightDefender.h"
#include "../Enemies/NightlightEnemy.h"

namespace
{
	// Enemies and defenders both have IsDead, so the same helpers serve both lists.
	template <typename ActorType>
	bool AddEntry(TArray<TWeakObjectPtr<ActorType>>& Entries, ActorType* const Actor)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		// Stale entries are cleared here as well, so the list cannot grow if an EndPlay is ever missed.
		Entries.RemoveAll([](const TWeakObjectPtr<ActorType>& Entry) { return !Entry.IsValid(); });
		if (Entries.Contains(Actor))
		{
			return false;
		}

		Entries.Add(Actor);
		return true;
	}

	template <typename ActorType>
	bool RemoveEntry(TArray<TWeakObjectPtr<ActorType>>& Entries, ActorType* const Actor)
	{
		if (!Actor)
		{
			return false;
		}

		const int32 RemovedCount = Entries.Remove(Actor);
		Entries.RemoveAll([](const TWeakObjectPtr<ActorType>& Entry) { return !Entry.IsValid(); });
		return RemovedCount > 0;
	}

	template <typename ActorType>
	TArray<ActorType*> GetLivingEntries(const TArray<TWeakObjectPtr<ActorType>>& Entries)
	{
		TArray<ActorType*> LivingActors;
		for (const TWeakObjectPtr<ActorType>& Entry : Entries)
		{
			ActorType* const Actor = Entry.Get();
			if (IsValid(Actor) && !Actor->IsDead())
			{
				LivingActors.Add(Actor);
			}
		}

		return LivingActors;
	}

	template <typename ActorType>
	ActorType* FindClosestLivingEntry(
		const TArray<TWeakObjectPtr<ActorType>>& Entries,
		const FVector& Location,
		const float Range)
	{
		float ClosestDistanceSquared = FMath::Square(FMath::Max(Range, 0.0f));
		ActorType* ClosestActor = nullptr;

		for (const TWeakObjectPtr<ActorType>& Entry : Entries)
		{
			// A destroyed actor makes the weak pointer return null instead of a dangling pointer
			// (Epic Games, Inc., 2026d).
			ActorType* const Actor = Entry.Get();
			if (!IsValid(Actor) || Actor->IsDead())
			{
				continue;
			}

			// Distance is measured flat, so a defender on its crate or a floating unit is not put out of
			// reach by its height (Epic Games, Inc., 2026b).
			const float DistanceSquared = FVector::DistSquared2D(Location, Actor->GetActorLocation());
			if (DistanceSquared <= ClosestDistanceSquared)
			{
				ClosestActor = Actor;
				ClosestDistanceSquared = DistanceSquared;
			}
		}

		return ClosestActor;
	}
}

void UNightlightActorRegistrySubsystem::Deinitialize()
{
	// A world subsystem ends with its world, so nothing from this level can be carried into the next
	// (Epic Games, Inc., 2026c; Epic Games, Inc., 2026f).
	Enemies.Empty();
	Defenders.Empty();
	Super::Deinitialize();
}

void UNightlightActorRegistrySubsystem::RegisterEnemy(ANightlightEnemy* const Enemy)
{
	// GetAllActorsOfClass walks every actor of the class on each call, which becomes slow with large
	// waves (Epic Games, Inc., 2026e). Keeping a list only costs one add and one remove per enemy.
	if (AddEntry(Enemies, Enemy))
	{
		// Listeners such as the later wave director react without the enemy knowing about them
		// (Nystrom, 2014).
		OnEnemyRegistered.Broadcast(Enemy);
	}
}

void UNightlightActorRegistrySubsystem::UnregisterEnemy(ANightlightEnemy* const Enemy)
{
	if (RemoveEntry(Enemies, Enemy))
	{
		OnEnemyRemoved.Broadcast(Enemy);
	}
}

void UNightlightActorRegistrySubsystem::RegisterDefender(ANightlightDefender* const Defender)
{
	if (AddEntry(Defenders, Defender))
	{
		OnDefenderRegistered.Broadcast(Defender);
	}
}

void UNightlightActorRegistrySubsystem::UnregisterDefender(ANightlightDefender* const Defender)
{
	if (RemoveEntry(Defenders, Defender))
	{
		OnDefenderRemoved.Broadcast(Defender);
	}
}

TArray<ANightlightEnemy*> UNightlightActorRegistrySubsystem::GetEnemies() const
{
	return GetLivingEntries(Enemies);
}

TArray<ANightlightDefender*> UNightlightActorRegistrySubsystem::GetDefenders() const
{
	return GetLivingEntries(Defenders);
}

int32 UNightlightActorRegistrySubsystem::GetLivingEnemyCount() const
{
	int32 LivingCount = 0;
	for (const TWeakObjectPtr<ANightlightEnemy>& Entry : Enemies)
	{
		const ANightlightEnemy* const Enemy = Entry.Get();
		if (IsValid(Enemy) && !Enemy->IsDead())
		{
			++LivingCount;
		}
	}

	return LivingCount;
}

ANightlightEnemy* UNightlightActorRegistrySubsystem::FindClosestEnemy(const FVector Location, const float Range) const
{
	return FindClosestLivingEntry(Enemies, Location, Range);
}

ANightlightDefender* UNightlightActorRegistrySubsystem::FindClosestDefender(const FVector Location, const float Range) const
{
	return FindClosestLivingEntry(Defenders, Location, Range);
}

/*
References

Epic Games, Inc., 2026a. Dynamic Delegates in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/dynamic-delegates-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026b. FVector::DistSquared2D. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/Math/FVector/DistSquared2D>
[Accessed 2 October 2026].

Epic Games, Inc., 2026c. Programming Subsystems in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-subsystems-in-unreal-engine>
[Accessed 29 September 2026].

Epic Games, Inc., 2026d. TWeakObjectPtr. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/TWeakObjectPtr>
[Accessed 29 September 2026].

Epic Games, Inc., 2026e. UGameplayStatics::GetAllActorsOfClass. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameplayStatics/GetAllActorsOfClass>
[Accessed 29 September 2026].

Epic Games, Inc., 2026f. UWorldSubsystem. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorldSubsystem>
[Accessed 29 September 2026].

Nystrom, R., 2014. Game Programming Patterns: Observer. [online] Available at:
<https://gameprogrammingpatterns.com/observer.html> [Accessed 29 September 2026].
*/
