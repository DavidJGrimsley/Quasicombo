Here’s the clean summary of where we are and what you’re currently trying to achieve.

You’re building Quasicombo in Unreal Engine using the existing SideScrollingCharacter as the main player character. The repo is DavidJGrimsley/Quasicombo, and we’ve been editing main.

The first thing we did was merge useful behavior from the other template characters into the SideScroller. I added the Platforming character’s dash and the Combat character’s combat system directly into ASideScrollingCharacter, instead of changing your pawn to one of those other characters. That included the dash montage/action, combo attacks, charged attacks, damage, knockback, healing, death, respawn behavior, combat interfaces, life bar support, and relevant animation notify support.

You then wired the new Blueprint-callable functions into BP_SideScrollingCharacter, including things like:

Do Dash
Do Combo Attack Start
Do Combo Attack End
Do Charged Attack Start
Do Charged Attack End

We also dealt with the touch-interface confusion. The BPI_TouchInterface_* assets are just Unreal template support for mobile/touch controls. You did not accidentally make a mobile game. Since you’re targeting desktop, those touch functions are optional.

We hit a C++ build problem because your .uproject had:

VisualStudioTools

enabled, and UE 5.8 was failing before it even reached our game code. Disabling that plugin fixed the build.

After that, you asked for a gameplay combo counter.

I added that functionality directly to ASideScrollingCharacter.

The current C++ combo system already does this:

Player successfully damages something
        ↓
RegisterHitCombo()
        ↓
HitComboCount++
        ↓
Restart combo timeout
        ↓
No successful hit for 5 seconds
        ↓
ResetHitCombo()
        ↓
HitComboCount = 0

The 5-second duration is configurable in the SideScroller character defaults:

Combat
└── Hit Combo
    └── Hit Combo Reset Delay

Default:

5.0 seconds

I also exposed these functions to Blueprint:

Get Hit Combo Count
Get Hit Combo Time Remaining
Register Hit Combo
Reset Hit Combo

Importantly, you do not need to detect hits in Blueprint. The existing C++ attack trace already detects when the attack hits something damageable and calls:

RegisterHitCombo()

automatically.

So right now your job is purely the HUD/UMG side.

You created HUD functions including:

UpdateComboCount
UpdateComboSecondsRemaining
ToggleSecondsVisibility

Your UpdateComboCount graph is basically correct:

PC
→ Get Hit Combo Count
→ Set Current Combo Count
→ Convert Integer to Text
→ Set Combo Count Text
→ Current Combo Count > 0
→ Toggle Seconds Visibility

Your latest UpdateComboSecondsRemaining graph is also now correct:

PC
→ Get Hit Combo Time Remaining
→ To Text (Float)
→ Set Seconds Remaining Text

You currently have the float configured for one decimal place, so it should display something like:

4.9
4.8
4.7
...

You also created a cached PC variable containing your BP_SideScrollingCharacter, which is good. That means you only need to cast once when the HUD is constructed instead of repeatedly casting in every function.

The one main missing piece right now is making those HUD update functions run continuously.

Your intended Event Construct setup should be:

Event Construct
    ↓
Get Owning Player Pawn
    ↓
Cast to BP_SideScrollingCharacter
    ↓
Set PC
    ↓
Set Timer by Event

Configure that timer as:

Time: 0.1
Looping: TRUE

The red Event input of Set Timer by Event needs to point to a Custom Event, for example:

RefreshComboHUD

Then:

RefreshComboHUD
    ↓
UpdateComboCount
    ↓
UpdateComboSecondsRemaining

That means every tenth of a second your HUD asks the character:

What is the current combo?
How many seconds remain?

and refreshes the display.

So the overall architecture is now:

C++ / SideScrollingCharacter
│
├── detects successful attacks
├── increments combo
├── manages 5-second timeout
├── resets combo
└── exposes combo data to Blueprint
             │
             ▼
          UMG HUD
             │
             ├── UpdateComboCount
             ├── UpdateComboSecondsRemaining
             └── ToggleSecondsVisibility

You also created an OnHitComboChanged event dispatcher, but at the moment you don't need it. The C++ code is not broadcasting that Blueprint dispatcher, and the simple repeating 0.1s HUD timer is enough for now.

So your immediate goal is:

Finish the HUD refresh timer so the combo count and countdown update live on screen.

You are not rebuilding the combo system from scratch. The gameplay side is already implemented; you're just finishing the visual display.