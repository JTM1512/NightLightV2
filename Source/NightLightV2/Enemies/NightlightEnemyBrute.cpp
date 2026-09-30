#include "NightlightEnemyBrute.h"

ANightlightEnemyBrute::ANightlightEnemyBrute()
{
	// Starting values for a slow tank: half the walker's speed, four times its health and three times
	// its Core damage. Tune them in BP_EnemyBrute.
	MovementSpeed = 150.0f;
	MaxHealth = 400.0f;
	CurrentHealth = 400.0f;
	CoreDamage = 30.0f;

	// A short reach, so the Brute has to walk up to defenders before it can hit them.
	DefenderAttackRange = 300.0f;
	DefenderAttackDamage = 20.0f;
	DefenderAttackInterval = 2.0f;

	// The hardest enemy to kill is worth the most tokens.
	TokensOnDeath = 40;
}
