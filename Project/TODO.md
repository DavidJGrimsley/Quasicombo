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
- [ ] Play the different versions of the background music during the boss fight and it's stages

Boss and QTE
- [x] Figure out how the braid is affecting the boss, how the time evolution is effecting the boss, and how we could make that more dramatic and apparent through the use of different sounds/versions of the background music and multiple round of QTE where the boss maybe gets x additonal health bars based on the time evolution and x rounds of QTE based on the braid measurement but idk, just suggestions. Right now it's easy which means the player could do multipler rounds if need be.
- [ ] Refine QTE Camera cuts after final asset or at the end if asset's dont' make it.
- [ ] Pressing QTE buttons does nothing. Each of the 3-5 buttons should map to the 3-5 combo attack animations the mike robot player character already has. the third one, the kick, should be in slow motion and 

Combat
- [x] can't attack mid air without it stopping forward velocity. We want the player to be able to jump (or shoot) in the air. in fact let's make it so the player ignores gravity for the duration of the attacking animation.
- [x] animation of a punch in the air looks shitty -> probably need a state machine
- [ ] The character won't do the kick animation for the third combo hit. This was lost when adding mike in cause he doesn't have a skeleton and has his own attacks. I'd like to see Mike do a drop kick as dash attack.
- [x] Attacking mid dash cancels the dash when it should do a new dash attack/drop kick. (The drop kick part might need to be deferred to an animation/rigging specialist - idk. we'll see.)
- [x] Change punch button to face pad right (B)
- [x] Fix punch animation so the player doesn't get stalled when spamming the punch button but also speed up the entire punch animations (and effects) by 20% so the combos are faster.
- [x] The third attack in a combo should do 1.5 times damage as the first two

Cannon Blaster
- [ ] Cannon Blaster is an armament that comes with mike the robot asset pack. we want it to fire a projectile. The projectile should be the same color as the lights of the robot - whatever it is we decide to go with. Firing will be done with gamepad left (X - scroll click on desktop.). Projectile will do twice as much damage as the attack
- [ ] Cannon blaster is disabled/unequipped on first run of the game.
- [ ] Place cannon blaster as a pickup when you defeat the boss and then present 3 Victory screen.

- [x] HUD
- - After beating the level, show a Score at top middle of HUD.  Score will be a combination of the speed times the highest combo or something. We need to figure out a way to reward players with a shorter time, not a longer time so straight multiplication won't work. The score should also increase based on how braided their route was.
- - Combo is repeated unnecessarily in the hud.
- - let's remove hits remaining for boss and enemy. 
- - Let's remove this confusing shit above the A B AND C lines (it can be explained in the technical info described below). KEEP THE braid LINES. Remove the two lines of text agove them.
- [x] Present 3 options on the you win screen:
- - See technical info and credits (need to make screens for this that explain how the quantum braid worked in the game generally and specifically for that run. Then roll credits I will provide in project/credits.md)
- - Replay level with cannon blaster (need to make functionality)
- - Quit (need to make functionality)

Enemies & Combat 
- [ ] Seek advice from a combat specialist for feel and balance
- [ ] Add kick attack for enemis which shoul trigger instead of a punch if the player character is out of reach for punch so maybe the height of the PC is under half-height of enemy, the enemy should kick instead of punch.
- [ ] Add stomp attack for heavy variants that does 1.5 damage.

Final Assets
- [x] add golem enemy variants for light, medium(new), and heavy. add a wood monster variant for medium as well. each of those character assets have 3 different color schems so it should work out well. brown/light green = light. white-ish/green = medium. fire-red/red = heavy. respectively golem-color/wood-monster-color. medium and heavy variants should be slightly larger than the variant before it aka small medium large. only like scale of 1, 1.125, and 1.25
- [x] replace enemy mannequins with wood monsters
- [x] replace boss mannequin with monster
- [ ] Design Environment
  - [x] background
  - [x] semi background
  - [ ] playable area
  - [ ] PCG
  - [ ] foreground & plants