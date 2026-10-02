#include "NightlightWaveSettings.h"

namespace
{
	FNightlightWaveEnemyEntry MakeEnemyEntry(
		const ENightlightWaveEnemyRole Role,
		const int32 ThreatCost,
		const int32 UnlockWave,
		const float BaseWeight)
	{
		FNightlightWaveEnemyEntry Entry;
		Entry.Role = Role;
		Entry.ThreatCost = ThreatCost;
		Entry.UnlockWave = UnlockWave;
		Entry.BaseWeight = BaseWeight;
		return Entry;
	}

	FNightlightWaveTemplate MakeTemplate(
		const FName Name,
		const int32 UnlockWave,
		const float Walker,
		const float Shade,
		const float Brute,
		const ENightlightWaveTemplateCounter Counters)
	{
		FNightlightWaveTemplate Template;
		Template.Name = Name;
		Template.UnlockWave = UnlockWave;
		Template.Weights.Walker = Walker;
		Template.Weights.Shade = Shade;
		Template.Weights.Brute = Brute;
		Template.Counters = Counters;
		return Template;
	}
}

UNightlightWaveSettings::UNightlightWaveSettings()
{
	// The three rows from the planning document. Their classes stay empty so the editor picks the Blueprint
	// children; a row without a class is never spawned (Epic Games, Inc., 2026a).
	Enemies.Add(MakeEnemyEntry(ENightlightWaveEnemyRole::Walker, 1, 1, 1.0f));
	Enemies.Add(MakeEnemyEntry(ENightlightWaveEnemyRole::Shade, 2, 3, 0.6f));
	Enemies.Add(MakeEnemyEntry(ENightlightWaveEnemyRole::Brute, 4, 5, 0.3f));

	// Swarm floods long-range builds, Skirmish's Shades outrange short-range builds and Siege's Brutes hit
	// packed builds with area damage, so the waves adapt to how the player builds (Hunicke and Chapman, 2004).
	Templates.Add(MakeTemplate(TEXT("Swarm"), 1, 2.0f, 1.0f, 1.0f, ENightlightWaveTemplateCounter::LongRange));
	Templates.Add(MakeTemplate(TEXT("Skirmish"), 3, 1.0f, 2.0f, 1.0f, ENightlightWaveTemplateCounter::ShortRange));
	Templates.Add(MakeTemplate(TEXT("Siege"), 5, 1.0f, 1.5f, 2.0f, ENightlightWaveTemplateCounter::Packed));
}

/*
References

Booth, M., 2009. The AI systems of Left 4 Dead. [pdf] Bellevue: Valve Corporation. Available at:
<https://cdn.fastly.steamstatic.com/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf> [Accessed 30 September 2026].

Chen, J., 2007. Flow in games (and everything else). Communications of the ACM, [e-journal] 50(4), pp.31-34. Available at:
<https://khoury.northeastern.edu/~lieber/courses/csu670/f08/materials/p31-chen-flow-in-games.pdf> [Accessed 30 September 2026].

Epic Games, Inc., 2026a. Data Assets in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/data-assets-in-unreal-engine>
[Accessed 1 October 2026].

Epic Games, Inc., 2026b. Structs in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/structs-in-unreal-engine>
[Accessed 1 October 2026].

Hunicke, R. and Chapman, V., 2004. AI for dynamic difficulty adjustment in games. [pdf] Evanston: Northwestern University. Available at:
<https://users.cs.northwestern.edu/~hunicke/pubs/Hamlet.pdf> [Accessed 30 September 2026].
*/
