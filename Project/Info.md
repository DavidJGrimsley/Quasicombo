QUASICOMBO

1. Game Overview
Genre: 2.5D side-scrolling beat-'em-up with quantum-braiding route choices and boss QTEs
Engine: Unreal Engine 5; C++ project with Blueprint-first gameplay
Camera / Movement: 3D world presented as a side-scroller; movement is primarily left/right
Player: Mike, a customizable robot
Enemies: Wood Monsters and Stone Golems
Goal: Fight through branching routes, build a braid history, and defeat quantum-influenced bosses

2. High Concept
Quasicombo is a short 2.5D beat-'em-up where the player's route through the level creates an ordered quantum braid. Normal play focuses on melee combat and combos. At route junctions, the player chooses a path; each meaningful crossing is recorded as part of the braid. At boss encounters, the accumulated braid is evaluated by the Quantum API and influences boss states and QTE behavior.

3. Core Gameplay
Combat
- Simple beat-'em-up movement, 1 button combo attacks, cannon projectile (maybe), dash, and double jump.
- Combo counter rises with consecutive hits and resets after a configurable timeout.
- Enemies on the current route must be cleared before advancing.

Level / Route Structure
- The game uses three braid strands/routes: A, B, and C.
- The player clears only the chosen route for that section.
- Once a route is committed to, the alternate routes for that section are locked.
- Later crossings can move the player onto another strand.
- Each meaningful crossing adds one operation to the ordered braid history.

Bosses and QTEs
- Bosses appear after groups of combat sections.
- The accumulated braid is evaluated when a boss encounter begins or changes phase.
- Quantum probabilities influence the boss's state or behavior.
- At selected health thresholds, a QTE acts as a measurement/readout moment.
- The quantum result determines the situation or prompt pattern; player execution determines the payoff.
- Boss state can also evolve during combat through the Quantum API time-evolution endpoint.

4. Core Loop
1. Enter a combat section.
2. Defeat enemies and build the combo counter.
3. Reach a braid junction and choose a route.
4. Record the braid operation created by that crossing.
5. Repeat through additional sections.
6. Reach a boss and evaluate the accumulated braid.
7. Fight the boss and complete quantum-driven QTEs.

5. Characters and Environment
Player Character: Radical Robots - Mike the Customizable Robot - primary playable robot
Environment: Scifi Jungle Biome - main alien / science-fiction jungle setting
Enemy: Wood Monster - standard monster enemy
Enemy: Stone Golem - heavy enemy; may also serve as an elite or boss

Asset references:
https://www.fab.com/listings/c366c6f1-a0af-4083-8d16-0a08ead490c7
https://www.fab.com/listings/6997ac35-3f01-4915-9427-40ccb33babe6
https://www.fab.com/listings/c123025e-bf13-4f87-8fc9-40cf1eafedab
https://www.fab.com/listings/89b16c3a-47ea-486e-ac09-bc2237af93f9

6. Narrative


7. Technical - Quantum Braiding
The game hides the quantum math from normal gameplay. Unreal stores the player's route choices and sends the ordered braid history to the Quantum API.

Three-Strand Model
- A, B, and C are the three strands/routes.
- A/B crossings use generator 1; B/C crossings use generator 2.
- Crossing direction is stored as power 1 or -1.
- Order matters: changing the order of crossings can change the quantum result.

Game Data
Example braid history: [{generator: 1, power: 1}, {generator: 2, power: -1}, ...]

Quantum API Flow
- A persistent Unreal manager stores the braid history.
- At a boss/readout point, Unreal calls POST /v1/topological/braid through the Quantum API Unreal plugin.
- The API returns fusion probabilities and, when requested, a sampled measurement result.
- Blueprint gameplay converts the result into boss state, QTE setup, VFX, and rewards.
- Boss combat can separately call POST /v1/algorithms/time_evolution for quantum state changes over time.

Rules / Fallback
- Enemy kills are normal gameplay and are not part of the braid calculation.
- The player does not need to clear every route; only the chosen path contributes to the route history.
- Quantum API code stays generic; Quasicombo-specific boss and QTE rules stay in the Unreal project.
- If the API is unavailable, use a deterministic local fallback so the game never hard-stops.

8. Jam MVP
- One playable robot.
- Two enemy types.
- Three routes/strands with multiple braid junctions.
- Basic combat and combo counter.
- One boss encounter.
- At least one braid evaluation and one QTE measurement/readout.

