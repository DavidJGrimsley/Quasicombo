Roadmap
Base of game
- [x] Add dash from platformer character to sidescroller
- [x] Add combat from combat character to sidescroller
- [x] Add enemy and test combat - needs to be smoother/faster - notes are below
After choosing style/asset library
- [x] Add 2 or 3 types of enemies
- [x] 1 type of boss
- [x] Wire up quantum shit 
- [x] Use xbox game pad button icons for QTE
- [x] jump trails don't work - should be same color as robot lights as well. these are from the platformer character. I copied and pasted them over but something didn't work right i guess.
- [x] Get font in game for combo display

Background music and sounds
- [x] Get someone to make sound effects...
- [x] Run Yoshi's piano music through the quantum jazz effect and put all versions in the content folder.
- [x] Get the orignal version background music playing normally when the game loads up
- [x] Play the different versions of the background music during the boss fight and it's stages
- [x] Play Mike's punch effects only on accepted contact with an enemy or punchable target.
- [ ] Supply and assign a Buster Cannon firing sound to the cannon component's `FireSound` slot; the firing hook is ready.

Enemy resistance
- [x] We need to make the enemies a bit harder cause right now as long as you keep punching, the enemies are just freefalling/can't fight back. this is great for the easy enemies but for all enemies it feels wrong. We may need a new variable for our enemies and bosses called resistance. Let's say they have 0 resistance right now and 1 means they are not phased at all by the player's punches. let's configure it the following way
- [x] Wood monsters - Light: 0, Medium .1, Heavy: 2.
- [x] Golems - Light: .3, Medium: .4, Heavy: .6
- [x] Boss - with no armor: .6, with all armor: 1.0 (the armor of the lizard can be none, some, some more, or all armor and this resistance value should scale betweeen .6 and 1.0 accordingly.


Enemies & Combat 
- [ ] Seek advice from a combat specialist for feel and balance. The resistance I added was an attempt to balance but it only slightly improved it. 
- [ ] Add kick attack for enemies which should trigger instead of a punch if the player character is out of reach for punch so maybe the height of the PC is under half-height of enemy, the enemy should kick instead of punch.
- [ ] Add stomp attack for heavy variants that does big damage.
- [ ] Fix the hitboxes or whatever for the enemies because it feels like I'm hitting them without actually making contact meaning they are farther than the player charcter's fist actually reaches but it still makes the punch work.
- [ ] Give mike a parry which is his chomp attack assigned to Y.

Boss and QTE
- [x] Figure out how the braid is affecting the boss, how the time evolution is effecting the boss, and how we could make that more dramatic and apparent through the use of different sounds/versions of the background music and multiple round of QTE where the boss maybe gets x additonal health bars based on the time evolution and x rounds of QTE based on the braid measurement but idk, just suggestions. Right now it's easy which means the player could do multipler rounds if need be.
- [x] Refine QTE Camera cuts after final asset or at the end if asset's dont' make it.
- [x] Pressing QTE buttons does nothing. Each of the 3-5 buttons should map to the 3-5 combo attack animations the mike robot player character already has. the last one, the kick, should be in slow motion and send the boss character flying if it's also the last round of qte, if not, the lizard man should actually hit the player and knock them backwards while taking away half of their health and then the fight resumes. The number of qte buttons to press should come from the quantum state or whatever as well. minimum 2 max 5. I want to use the chomp animation at least. Maybe we can do 1 jump -> 2 uppercut -> 3 chomp -> 4 claw attack -> 5 kick + slowmo flyback of enemy
- [x] The multiple rounds of QTE that have the chance of happenning aren't configuered correctly. it's not that I don't want multiple rounds but when there are multiple rounds, the boss's health should get reset fully after each round/QTE (other than the last round) and then the player has to get the health down to 0 again before the next QTE. So essentially the QTE restores the boss's health bar if it's not the last round. 
- [x] We need to adjust the on screen text to not be so obviously tacky and over explanatory.
- [x] during the time evolution phase there should be some on screen visuals to alert the player that something weird is happeneing. That visual should be that the camera focuses on the boss ass described below but also 3 auras from this kit: https://www.fab.com/listings/d0b1351a-acbf-4da6-ab4e-32fc934a255f should show on the lizardman at that time. The 3 auras being Trails of Death, Ring of Flames, and MagicalBarrier. You can use the 6,7, and 8 from before so i can easily test this out. The armor that gets equipped during this phase should animate on the lizardman while the camera cuts to zoom in on him while facing him instead of from the side (similiar to the way the animation is for when the player hits the buster cannon pickup when they defeat the boss. Also see this animation sequence for how I have the player floating while doing a special anim and at the same time the VFX is spawned.) - the lizard should do some sort of roar or something after all armor is equipped. The buffs that the lizard gets should appear at the top of the screen and stay there and should be easily readable. The lizard man should float upwards before each peice of armor fades in/materialjizes The extra health that gets applied should be a blue bar(s) that appears over the boss's health bar that we will call armor sheilds. The armor currently gets knocked off (disappears) as the player attacks but these meshes shouldn't disappear, they should fall to the ground in an animated matter. The armor shields should go down as the player hits them which matches the armor falling off. empty shield bars should disappear. Armor should also increase resistance as defined in the 'Enemy Resistance' section of this document. The resistance variable branch should already be down so it may just work already but we should confirm. The beginning of the sequence should be slow enough to wait for the time evolution results should come back by the time the lizard man has floated up and been in the air for roughly .5-1 second. The vfx should spawn around the lizard man as he is floating up. Then the time evolution results should appear on the lizard - a camera cut should first zoom in on it's face and then the eye and skin color should change. then the camera should zoom back only enough to see his whole body so the pieces of armor can spawn off camera and then fly into place, then the lizardman should land down and roar. I guess we're going to ahve to author some animations. 

Combat
- [x] can't attack mid air without it stopping forward velocity. We want the player to be able to jump (or shoot) in the air. in fact let's make it so the player ignores gravity for the duration of the attacking animation.
- [x] animation of a punch in the air looks shitty -> probably need a state machine
- [ ] The character won't do the kick animation for the third combo hit. This was lost when adding mike in cause he doesn't have a skeleton and has his own attacks. I'd like to see Mike do a drop kick as dash attack.
- [x] Attacking mid dash cancels the dash when it should do a new dash attack/drop kick. (The drop kick part might need to be deferred to an animation/rigging specialist - idk. we'll see.)
- [x] Change punch button to face pad right (B)
- [x] Fix punch animation so the player doesn't get stalled when spamming the punch button but also speed up the entire punch animations (and effects) by 20% so the combos are faster.
- [x] The third attack in a combo should do 1.5 times damage as the first two

Buster Cannon
- [x] Fire one color-matched energy projectile per gamepad X, keyboard F, or middle-mouse press. Each accepted projectile hit counts as one strike; the light color is shared with Mike and can be changed through `SetMikeLightColor`.
- [x] Start a fresh session with the Buster Cannon unequipped; retain it on replay and death retries during that session.
- [x] Acquire the Buster Cannon from the boss pickup with a 165 cm lift, two rising cannon orbits, equip animation, landing, and delayed victory screen.
- [x] Use the FX Variety Pack Water Ball launch, travel, and impact effects with Mike's shared light color; keep projectile collision and damage in the gameplay actor.
- [x] Visually tune barrel alignment, Thunder Ball diameter/tint, rapid-shot pose, and pickup timing in the normal level.

Menu
- [x] Add Mike light-color customization to the menu before starting a run.
- [x] Add Start menu which is an overlay on the main level with these options:
Start - Customize - Settings
Start menu should have the jungle sounds playing already and the background music should start when the player starts
By overlay I mean the player character should be there idle and every 10-20 seconds he should turn and wave at the camera. It should be zoomed in with the 3 menu items vertically stacked with a good amont of space in between them. the menu on the left and player character on the right. This layout will make it easy for the mike customization screen which should just replace the previous menu and let the player customize the two color slots.
When the player hits start, the menu should fade away, camera zooms out to normal and the game starts
Options should just have 'Replay story' option or anything else we can thinkg of but i got nothing rn. mabye like a game info or something.
- [x] Before start menu, show the narrative story in blocks of text while playing intro music
Editable Widget Blueprint menu, customization, settings and timing documentation: [Menu.md](Menu.md).

- [x] HUD
- - After beating the level, show a Score at top middle of HUD.  Score will be a combination of the speed times the highest combo or something. We need to figure out a way to reward players with a shorter time, not a longer time so straight multiplication won't work. The score should also increase based on how braided their route was.
- - Combo is repeated unnecessarily in the hud.
- - let's remove hits remaining for boss and enemy. 
- - Let's remove this confusing shit above the A B AND C lines (it can be explained in the technical info described below). KEEP THE braid LINES. Remove the two lines of text agove them.
- [x] Present 3 options on the you win screen:
- - See technical info and credits (need to make screens for this that explain how the quantum braid worked in the game generally and specifically for that run. Then roll credits I will provide in project/credits.md)
- - Replay level with Buster Cannon (reloads the level with the cannon equipped)
- - Quit (need to make functionality)

Random
- [x] Make a blueprint for roaming background monsters so they're not just standing there idle.
- [x] Use stepping turn animations where available and curved walking turns elsewhere; add a stationary Megaspikan Blueprint with 5–20 second roars and alternating 45-degree stepping readjustments about every 20 seconds.
- [x] Fix wall jump mechanic by adding stickiness and a wall slide animation which slows the fall and makes jumping off the wall easier. There's already some of this existing but it could be a little better like mario or samus.
- [x] Player character's health should only regenerate while the player is not 'in combat' instead of the current 'being attacked'. The difference being that currently if mike is attacking then his health goes up. The regenerating should happen 3 seconds after he gets hit or lands an attack of any kind.
- [x] Make the nearby Megaspikan larvae a guaranteed lethal emergence/spit trap, separate from roaming scenery.
- [x] Keep Mike dead through RUN LOST until the new run creates a living idle character.
- [ ] Does mike play his hurt and death animations? especially if you lose the run during boss fight we can play a special one if it exists.

Background controls and validation: [BackgroundGameplay.md](BackgroundGameplay.md). Builds, focused tests and live integration passed; physical keyboard/controller acceptance remains pending, and the separate Buster particle-color regression is documented there.


Final Assets
- [x] add golem enemy variants for light, medium(new), and heavy. add a wood monster variant for medium as well. each of those character assets have 3 different color schems so it should work out well. brown/light green = light. white-ish/green = medium. fire-red/red = heavy. respectively golem-color/wood-monster-color. medium and heavy variants should be slightly larger than the variant before it aka small medium large. only like scale of 1, 1.125, and 1.25
- [x] replace enemy mannequins with wood monsters
- [x] replace boss mannequin with monster
- [ ] Design Environment
  - [x] background
  - [x] semi background
  - [ ] playable area
    - [ ]  C - Bottom floor (needs small fixing - river and carve out viewable area for cave in section 3)
    - [ ]  B - Middle - mostly big branches
    - [ ]  C - Top - Mostly floating rocks.
  - [ ] PCG - fix floating jungle rocks and pcg actor error I'm getting. 
  - [ ] foreground & plants
- [ ] Footsteps vfx? https://www.fab.com/listings/49dc53b7-0a5f-4905-8951-e42aaf1135e4


Playtest bug fixes
- [x] the kill volumes(pits) should also kill enemies in case they fall in
- [x] When the boss fight starts, kill all othe enemies in the level
- [x] For the boss fight, when there are multiple rounds of QTE it is not clear to the user. we need to fix this by showing 'Rounds left: X' in the top of the screen. when x = 0, don't display it at all.
- [ ] Can we make part of the foreground trees semi transparent?
- [x] Back up camera for pits/in general.
