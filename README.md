# NightLight

A tower defence game in Unreal Engine 5.8. Enemies march on a Dream Core. You place defenders, and the next wave changes based on how the last one went.

## What I built

- A Dream Core with its own health. It strikes the closest enemy in range, and the run ends when the Core is destroyed.
- Defenders in standard, short-range, and long-range styles. The placed types are a shooter, a pulse, and a projectile.
- Enemy types that include a shade and a brute, plus enemy projectiles.
- A wave director that plans Swarm, Skirmish, and Siege waves. Every fifth wave is a larger siege. After each wave it scores how you played and makes the next one harder or easier.
- Procedural scenery and a world generator.
- A game HUD, pause menu, and game-over screen.

## Run it

1. Install Unreal Engine 5.8.
2. Clone this repo and open `NightLightV2.uproject`. Let the editor compile the C++ module.
3. Press Play. The startup map is `NightLightV2`.
