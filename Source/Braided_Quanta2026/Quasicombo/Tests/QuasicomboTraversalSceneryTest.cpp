#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "QuasicomboBackgroundCreature.h"
#include "QuasicomboAmbientRoarer.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "QuasicomboLarvaeTrap.h"
#include "QuasicomboPlayerMovement.h"
#include "QuasicomboRunSubsystem.h"
#include "SideScrollingCharacter.h"
#include "SideScrollingCombatEnemy.h"
#include "BusterProjectile.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMeshActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
void Advance(FTestWorldWrapper& World, float Seconds)
{
    const int32 Count = FMath::RoundToInt(Seconds / .01f);
    for (int32 I = 0; I < Count; ++I) World.TickTestWorld(.01f);
}
AStaticMeshActor* Block(UWorld* World, FVector Location, FVector Scale)
{
    auto* Actor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
    auto* Mesh = Actor->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Actor->SetActorScale3D(Scale);
    return Actor;
}
ASideScrollingCharacter* Mike(UWorld* World, FVector Location)
{
    UClass* Class = LoadClass<ASideScrollingCharacter>(nullptr, TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
    return Class ? World->SpawnActor<ASideScrollingCharacter>(Class, Location, FRotator::ZeroRotator) : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQCIdlePlacementTest, "Quasicombo.Background.IdlePlacementAndToggle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQCIdlePlacementTest::RunTest(const FString&)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
    auto* World = Fixture.GetTestWorld();
    Block(World, FVector(0,0,-50), FVector(60,60,1));
    auto* Actor = World->SpawnActor<AQuasicomboBackgroundCreature>(FVector(0,0,123), FRotator(5,39,7));
    Actor->SetActorScale3D(FVector(1.7,1.4,1.1));
    Actor->Roaming->bIdleOnly = true;
    Actor->OnConstruction(Actor->GetActorTransform());
    TestTrue(TEXT("Component idle defaults survive construction"), Actor->bIdleOnly && Actor->Roaming->bIdleOnly);
    Actor->bIdleOnly = false;
    Actor->OnConstruction(Actor->GetActorTransform());
    TestFalse(TEXT("Actor checkbox can override a component idle default"), Actor->Roaming->bIdleOnly);
    Actor->bIdleOnly = true;
    Actor->Roaming->bIdleOnly = true;
    Actor->Roaming->MinimumPause = Actor->Roaming->MaximumPause = .01f;
    const FTransform Authored = Actor->GetActorTransform();
    if (!Fixture.BeginPlayInTestWorld()) return false;
    Advance(Fixture, 2);
    TestTrue(TEXT("Idle preserves the complete placement"), Authored.Equals(Actor->GetActorTransform(), 0));
    TestFalse(TEXT("Idle does not start roaming"), Actor->Roaming->IsRoaming());
    Actor->bIdleOnly = false;
    Advance(Fixture, 5);
    TestTrue(TEXT("Unchecked creature moves on supported terrain"), FVector::Dist2D(Actor->GetActorLocation(), Authored.GetLocation()) > 1);
    TestTrue(TEXT("Local movement stays in its original radius"), FVector::Dist2D(Actor->GetActorLocation(), Authored.GetLocation()) <= 601);
    TestTrue(TEXT("Terrain movement retains the authored height offset"), FMath::IsNearlyEqual(Actor->GetActorLocation().Z, 123., .01));
    const FTransform AtToggle = Actor->GetActorTransform();
    Actor->Roaming->SetIdleOnly(true);
    Advance(Fixture, 1);
    TestTrue(TEXT("Component toggle freezes without teleporting"), AtToggle.Equals(Actor->GetActorTransform(), 0));
    Actor->Roaming->SetIdleOnly(false);
    Advance(Fixture, 2);
    TestTrue(TEXT("Resume retains original anchor"), Actor->Roaming->GetRoamingAnchor().Equals(Authored.GetLocation(), 0));
    Fixture.ForwardErrorMessages(this);
    return !Fixture.HasFailed();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQCUnsupportedSceneryTest, "Quasicombo.Background.UnsupportedGround", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQCUnsupportedSceneryTest::RunTest(const FString&)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
    auto* Actor = Fixture.GetTestWorld()->SpawnActor<AQuasicomboBackgroundCreature>(FVector(0,0,200), FRotator::ZeroRotator);
    Actor->Roaming->MinimumPause = Actor->Roaming->MaximumPause = .01f;
    const FTransform Placement = Actor->GetActorTransform();
    if (!Fixture.BeginPlayInTestWorld()) return false;
    Advance(Fixture, 2);
    TestTrue(TEXT("Unsupported scenery cannot wander or snap"), Placement.Equals(Actor->GetActorTransform(), 0));
    Fixture.ForwardErrorMessages(this);
    return !Fixture.HasFailed();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQCSlopedSceneryTest, "Quasicombo.Background.SlopedGroundAndObstacles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQCSlopedSceneryTest::RunTest(const FString&)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
    auto* World = Fixture.GetTestWorld();
    auto* Floor = Block(World, FVector(0,0,-50), FVector(80,80,1));
    Floor->SetActorRotation(FRotator(20,0,0));
    auto* Actor = World->SpawnActor<AQuasicomboBackgroundCreature>(FVector(0,0,123), FRotator::ZeroRotator);
    Actor->SetActorScale3D(FVector(6));
    Actor->Roaming->ObstacleRadius = 240;
    Actor->Roaming->RoamingRadius = 400;
    Actor->Roaming->MinimumPause = Actor->Roaming->MaximumPause = .01f;
    const FVector Anchor = Actor->GetActorLocation();
    if (!Fixture.BeginPlayInTestWorld()) return false;
    Advance(Fixture, 5);
    TestTrue(TEXT("Large probe does not mistake its supporting slope for an obstacle"), FVector::Dist2D(Actor->GetActorLocation(), Anchor) > 1);

    Actor->SetActorLocation(Anchor);
    Actor->Roaming->SetIdleOnly(true);
    Actor->Roaming->SetIdleOnly(false);
    for (float Side : {-1.f, 1.f})
    {
        Block(World, FVector(Side*300,0,0), FVector(.2,80,60));
        Block(World, FVector(0,Side*300,0), FVector(80,.2,60));
    }
    bool bStayedClear = true;
    for (int32 I = 0; I < 1000; ++I)
    {
        Fixture.TickTestWorld(.01f);
        const FVector Position = Actor->GetActorLocation();
        // Wall faces are 290 cm from the anchor; the 240 cm probe must stay inside.
        bStayedClear &= FMath::Abs(Position.X) < 51 && FMath::Abs(Position.Y) < 51;
    }
    TestTrue(TEXT("Ignoring supporting ground still rejects separate blocking geometry"), bStayedClear);
    Fixture.ForwardErrorMessages(this);
    return !Fixture.HasFailed();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQCSceneryTurningTest, "Quasicombo.Background.SteppedAndCurvedTurns", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQCSceneryTurningTest::RunTest(const FString&)
{
    for (float Side : {-1.f, 0.f, 1.f})
    {
        FTestWorldWrapper Fixture;
        if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
        auto* World = Fixture.GetTestWorld();
        Block(World, FVector(0,0,-50), FVector(60,60,1));
        auto* Actor = World->SpawnActor<AQuasicomboBackgroundCreature>(FVector(0,0,123), FRotator::ZeroRotator);
        auto* Roaming = Actor->Roaming.Get();
        auto* Body = Actor->GetSkeletalMeshComponent();
        Body->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Megaspikan/Meshes/SK_Megaspikan_Body.SK_Megaspikan_Body")));
        Roaming->IdleAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Megaspikan/Animations/ANIM_Megaspikan_Idle.ANIM_Megaspikan_Idle"));
        Roaming->WalkAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Megaspikan/Animations/ANIM_Megaspikan_WalkForward.ANIM_Megaspikan_WalkForward"));
        Roaming->MeshForwardYawOffset = -90;
        Roaming->MinimumPause = Roaming->MaximumPause = .01f;
        if (Side != 0)
        {
            Roaming->TurnLeftAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Megaspikan/Animations/ANIM_Megaspikan_Turn45Left.ANIM_Megaspikan_Turn45Left"));
            Roaming->TurnRightAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Megaspikan/Animations/ANIM_Megaspikan_Turn45Right.ANIM_Megaspikan_Turn45Right"));
        }
        if (!Fixture.BeginPlayInTestWorld()) return false;
        for (int32 I=0; I<100 && !Roaming->IsRoaming(); ++I) Fixture.TickTestWorld(.01f);
        if (!TestTrue(TEXT("Supported creature picks a destination"), Roaming->IsRoaming())) return false;
        const float DesiredYaw = (Roaming->GetRoamingDestination()-Actor->GetActorLocation()).Rotation().Yaw - 90;
        Actor->SetActorRotation(FRotator(0, DesiredYaw - (Side == 0 ? 120 : Side*100), 0));
        const FTransform BeforeTurn = Actor->GetActorTransform();
        if (Side != 0)
        {
            Advance(Fixture, .1f);
            TestTrue(TEXT("Dedicated clip owns an animated turn"), Roaming->IsTurning());
            auto* Clip = Side > 0 ? Roaming->TurnRightAnimation.Get() : Roaming->TurnLeftAnimation.Get();
            TestTrue(TEXT("Correct left/right stepping clip plays"), Body->GetSingleNodeInstance()->GetCurrentAsset() == Clip);
            Advance(Fixture, .5f);
            const float Turned = FMath::FindDeltaAngleDegrees(BeforeTurn.Rotator().Yaw, Actor->GetActorRotation().Yaw) * Side;
            TestTrue(TEXT("Turn advances gradually with the clip"), Turned > 1 && Turned < 45);
            TestTrue(TEXT("Stepped turn retains world placement"), BeforeTurn.GetLocation().Equals(Actor->GetActorLocation(), 0));
            Roaming->SetIdleOnly(true);
            const FTransform Frozen = Actor->GetActorTransform();
            Advance(Fixture, .5f);
            TestFalse(TEXT("Idle Only cancels animated turning"), Roaming->IsTurning());
            TestTrue(TEXT("Idle toggle freezes the turn without teleporting"), Frozen.Equals(Actor->GetActorTransform(), 0));
        }
        else
        {
            int32 CurvedSteps = 0;
            bool bNoIdleSwivel = true;
            bool bForwardSteps = true;
            for (int32 I=0; I<150; ++I)
            {
                const FVector Previous = Actor->GetActorLocation();
                const float PreviousYaw = Actor->GetActorRotation().Yaw;
                Fixture.TickTestWorld(.01f);
                const FVector Step = Actor->GetActorLocation()-Previous;
                const float YawChange = FMath::Abs(FMath::FindDeltaAngleDegrees(PreviousYaw,Actor->GetActorRotation().Yaw));
                if (YawChange > .01f)
                {
                    ++CurvedSteps;
                    bNoIdleSwivel &= Step.SizeSquared2D() > .01f && Body->GetSingleNodeInstance()->GetCurrentAsset() == Roaming->WalkAnimation;
                    bForwardSteps &= FVector::DotProduct(Step.GetSafeNormal2D(), Actor->GetActorRotation().RotateVector(FVector::RightVector).GetSafeNormal2D()) > .99f;
                }
            }
            TestTrue(TEXT("Fallback turns through multiple walking steps"), CurvedSteps > 5);
            TestTrue(TEXT("Fallback never rotates a stationary idle pose"), bNoIdleSwivel);
            TestTrue(TEXT("Curved steps follow the creature's facing"), bForwardSteps);
            TestTrue(TEXT("Walking arc remains inside its anchor radius"), FVector::Dist2D(Actor->GetActorLocation(),Roaming->GetRoamingAnchor()) <= Roaming->RoamingRadius+1);
        }
        Fixture.ForwardErrorMessages(this);
        if (Fixture.HasFailed()) return false;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQCAmbientRoarTest, "Quasicombo.Background.AmbientRoarIntervals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQCAmbientRoarTest::RunTest(const FString&)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
    auto* World = Fixture.GetTestWorld();
    auto* Actor = World->SpawnActor<AQuasicomboAmbientRoarer>(FVector(73,91,123),FRotator(8,37,11));
    Actor->SetActorScale3D(FVector(2));
    auto* Body = Actor->GetSkeletalMeshComponent();
    Body->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Megaspikan/Meshes/SK_Megaspikan_Body.SK_Megaspikan_Body")));
    const FTransform Placement = Actor->GetActorTransform();
    if (!Fixture.BeginPlayInTestWorld()) return false;
    auto* Roar = Actor->AmbientRoar.Get();
    TestTrue(TEXT("First roar waits a random 5-20 seconds"), Roar->TimeUntilNextRoar >= 5 && Roar->TimeUntilNextRoar <= 20);
    TestFalse(TEXT("Roarer starts idle"),Roar->IsRoaring());
    bool bWasRoaring = false, bWasReadjusting = false, bStationary = true;
    float LastStart = 0;
    float LastTurnStart = 0;
    TArray<float> Intervals;
    int32 IdleReturns = 0, TurnStarts = 0, TurnReturns = 0;
    for (int32 I=0; I<8000; ++I)
    {
        Fixture.TickTestWorld(.01f);
        bStationary &= Placement.GetLocation().Equals(Actor->GetActorLocation(),0) && Placement.GetScale3D().Equals(Actor->GetActorScale3D(),0);
        const FRotator Facing = Actor->GetActorRotation();
        const FRotator Authored = Placement.Rotator();
        const float YawOffset = FMath::FindDeltaAngleDegrees(Authored.Yaw,Facing.Yaw);
        TestTrue(TEXT("Readjustments alternate within 45 degrees of authored facing"), YawOffset >= -45.01f && YawOffset <= .01f);
        TestTrue(TEXT("Readjustments preserve authored tilt"), FMath::IsNearlyEqual(Facing.Pitch,Authored.Pitch,.01f) && FMath::IsNearlyEqual(Facing.Roll,Authored.Roll,.01f));
        TestFalse(TEXT("Roars and stepping turns never overlap"), Roar->IsRoaring() && Roar->IsReadjusting());
        if (Roar->IsReadjusting() && !bWasReadjusting)
        {
            const float Now = World->GetTimeSeconds();
            TestTrue(TEXT("Readjustment waits 20 seconds, finishing any active roar"), Now-LastTurnStart >= 19.99f && Now-LastTurnStart <= 26.1f);
            LastTurnStart = Now;
            const UAnimSequence* Expected = TurnStarts % 2 == 0 ? Roar->TurnLeftAnimation.Get() : Roar->TurnRightAnimation.Get();
            TestTrue(TEXT("Readjustment alternates left and right stepping clips"), Body->GetSingleNodeInstance()->GetCurrentAsset() == Expected);
            ++TurnStarts;
        }
        if (!Roar->IsReadjusting() && bWasReadjusting)
        {
            ++TurnReturns;
            TestTrue(TEXT("Complete stepping turn returns to idle"), Body->GetSingleNodeInstance()->GetCurrentAsset() == Roar->IdleAnimation);
            TestTrue(TEXT("Completed turn reaches alternate facing"), FMath::IsNearlyEqual(YawOffset,TurnReturns % 2 == 1 ? -45.f : 0.f,.01f));
        }
        if (Roar->IsRoaring() && !bWasRoaring)
        {
            const float Now = World->GetTimeSeconds();
            Intervals.Add(Now-LastStart);
            LastStart = Now;
            TestTrue(TEXT("Roar plays its one-shot animation"),Body->GetSingleNodeInstance()->GetCurrentAsset() == Roar->RoarAnimation);
        }
        if (!Roar->IsRoaring() && bWasRoaring)
        {
            ++IdleReturns;
            TestTrue(TEXT("Full roar finishes before returning to idle"), FMath::IsNearlyEqual(World->GetTimeSeconds()-LastStart,6.f,.05f));
            TestTrue(TEXT("Idle loop resumes after roar"),Body->GetSingleNodeInstance()->GetCurrentAsset() == Roar->IdleAnimation);
        }
        bWasRoaring = Roar->IsRoaring();
        bWasReadjusting = Roar->IsReadjusting();
    }
    TestTrue(TEXT("Roars repeat and return to idle"),Intervals.Num() >= 3 && IdleReturns >= 3);
    for (float Interval : Intervals) TestTrue(TEXT("Scheduled 5-20 second roars finish any active stepping turn"),Interval >= 4.99f && Interval <= 22.75f);
    TestTrue(TEXT("Stepping readjustments repeat"),TurnStarts >= 3 && TurnReturns >= 2);
    TestTrue(TEXT("Roaring and readjusting preserve location and scale"),bStationary);
    Actor->Destroy();
    TestFalse(TEXT("Teardown clears roar state"),Roar->IsRoaring());
    TestFalse(TEXT("Teardown clears stepping state"),Roar->IsReadjusting());
    Fixture.ForwardErrorMessages(this);
    return !Fixture.HasFailed();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQCWallPhysicsTest, "Quasicombo.Player.WallGripSlideJump", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQCWallPhysicsTest::RunTest(const FString&)
{
    for (float Side : {-1.f, 1.f})
    {
        FTestWorldWrapper Fixture;
        if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
        auto* World = Fixture.GetTestWorld();
        auto* Player = Mike(World, FVector(0,0,1000));
        if (!TestNotNull(TEXT("Mike"), Player)) return false;
        auto* Movement = Cast<UQuasicomboPlayerMovement>(Player->GetCharacterMovement());
        if (!TestNotNull(TEXT("Native wall movement"), Movement)) return false;
        const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
        Block(World, FVector(Side * (Radius+18), 0, 0), FVector(.2,10,100));
        if (!Fixture.BeginPlayInTestWorld()) return false;
        Movement->bRunPhysicsWithNoController = true;
        Movement->SetMovementMode(MOVE_Falling);
        Movement->Velocity = FVector(0,0,-700);
        Advance(Fixture, .05f);
        TestTrue(TEXT("Descending player grips either wall"), Movement->WallState == EQuasicomboWallState::Gripping);
        TestTrue(TEXT("Grip arrests descent"), FMath::Abs(Movement->Velocity.Z) < 1);
        Advance(Fixture, .25f);
        TestTrue(TEXT("Grip changes into slide"), Movement->WallState == EQuasicomboWallState::Sliding);
        TestTrue(TEXT("Falling physics caps wall descent"), Movement->Velocity.Z >= -120.01 && Movement->Velocity.Z < 0);
        Player->DoMove(0);
        Player->DoJumpStart();
        Advance(Fixture, .01f);
        TestTrue(TEXT("Neutral wall jump launches away and up"), Movement->Velocity.X * Side < -400 && Movement->Velocity.Z > 500);
        TestFalse(TEXT("Jump clears wall state"), Movement->IsOnWall());

        Advance(Fixture, .25f);
        Player->SetActorLocation(FVector(0,0,1000));
        Movement->ClearWall(0);
        Movement->Velocity = FVector(0,0,-300);
        Advance(Fixture, .02f);
        Player->SetActorLocation(FVector(-Side*40,0,1000));
        Advance(Fixture, .03f);
        Player->DoJumpStart();
        Advance(Fixture, .01f);
        TestTrue(TEXT("Lost contact retains a short neutral jump grace"), Movement->Velocity.X * Side < -400 && Movement->Velocity.Z > 500);

        Advance(Fixture, .25f);
        Player->SetActorLocation(FVector(0,0,1000));
        Movement->ClearWall(0);
        Movement->Velocity = FVector(0,0,-300);
        Advance(Fixture, .02f);
        Player->DoMove(-Side);
        Advance(Fixture, .02f);
        TestFalse(TEXT("Pressing away releases wall grip"), Movement->IsOnWall());
        Player->DoMove(0);
        Advance(Fixture, .05f);
        TestFalse(TEXT("Deliberate release cannot reattach immediately"), Movement->IsOnWall());
        Player->SetQTEComboHeld(true);
        TestFalse(TEXT("QTE hold clears wall state"), Movement->IsOnWall());
        Fixture.ForwardErrorMessages(this);
        if (Fixture.HasFailed()) return false;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQCLarvaeCaptureTest, "Quasicombo.Trap.CaptureAndLethalImpact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQCLarvaeCaptureTest::RunTest(const FString&)
{
    for (float Damage : {0.f, 7.f})
    {
        FTestWorldWrapper Fixture;
        if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
        auto* World = Fixture.GetTestWorld();
        auto* Player = Mike(World, FVector(300,0,200));
        auto* Actor = World->SpawnActor<AQuasicomboLarvaeTrap>(FVector(0,0,200), FRotator(17,33,25));
        if (!TestNotNull(TEXT("Mike"), Player) || !Fixture.BeginPlayInTestWorld()) return false;
        Player->GetCharacterMovement()->DisableMovement();
        if (Damage) Player->TakeDamage(Damage, FDamageEvent(), nullptr, nullptr);
        auto* Trap = Actor->Trap.Get();
        Player->DoDash();
        TestTrue(TEXT("Trap accepts a live nearby player"), Trap->TryTrigger(Player));
        const FVector MeshForward = Actor->GetActorRotation().RotateVector(FVector::RightVector);
        TestTrue(TEXT("Tilted larvae jaws face the captured player"), FVector::DotProduct(MeshForward.GetSafeNormal2D(), (Player->GetActorLocation() - Actor->GetActorLocation()).GetSafeNormal2D()) > .99f);
        TestTrue(TEXT("Capture preserves authored pitch and roll"), FMath::IsNearlyEqual(Actor->GetActorRotation().Pitch, 17., .01) && FMath::IsNearlyEqual(Actor->GetActorRotation().Roll, 25., .01));
        TestFalse(TEXT("Repeated activation is ignored"), Trap->TryTrigger(Player));
        TestTrue(TEXT("Trap owns a distinct input lock"), Player->IsInFatalTrap());
        const float HP = Player->GetCurrentHP();
        Player->DoJumpStart(); Player->DoDash(); Player->DoComboAttackStart(); Player->FireBuster();
        TestTrue(TEXT("Captured gameplay input cannot move Mike"), Player->GetCharacterMovement()->MovementMode == MOVE_None);
        Advance(Fixture, 1.95f);
        TestTrue(TEXT("Full emergence plays before spit"), Trap->State == EQuasicomboLarvaeTrapState::Emerging);
        TestTrue(TEXT("Capture suspends regeneration"), FMath::IsNearlyEqual(Player->GetCurrentHP(), HP));
        Advance(Fixture, .1f);
        TestTrue(TEXT("Spit follows emergence"), Trap->State == EQuasicomboLarvaeTrapState::Spitting);
        Advance(Fixture, .65f);
        TestEqual(TEXT("Scripted impact kills at full and partial health"), Player->GetCurrentHP(), 0.f);
        TestTrue(TEXT("Lethal impact executes once"), Trap->HasDeliveredLethalHit());
        TestFalse(TEXT("Death releases trap lock"), Player->IsInFatalTrap());
        if (auto* Run = World->GetSubsystem<UQuasicomboRunSubsystem>())
            TestTrue(TEXT("Normal player death declares defeat"), Run->GetOutcome() == EQuasicomboRunOutcome::Defeat);
        Advance(Fixture, .2f);
        TestTrue(TEXT("Spit finishes before reverse emergence"), Trap->State == EQuasicomboLarvaeTrapState::Burrowing);
        Fixture.ForwardErrorMessages(this);
        if (Fixture.HasFailed()) return false;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQCLarvaeEntryTest, "Quasicombo.Trap.EntryRearmAndCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQCLarvaeEntryTest::RunTest(const FString&)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
    auto* World = Fixture.GetTestWorld();
    auto* Player = Mike(World, FVector(900,0,200));
    auto* Actor = World->SpawnActor<AQuasicomboLarvaeTrap>(FVector(0,0,200), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Mike"), Player) || !Fixture.BeginPlayInTestWorld()) return false;
    auto* Controller = World->SpawnActor<APlayerController>();
    Controller->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    Controller->SetAsLocalPlayerController();
    Controller->Possess(Player);
    Player->GetCharacterMovement()->DisableMovement();
    auto* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
    if (Run) Run->HoldForFrontend();
    Player->SetActorLocation(FVector(300,0,200));
    Advance(Fixture, .1f);
    TestFalse(TEXT("Menu cannot trigger the trap"), Player->IsInFatalTrap());
    if (Run) Run->StartRun();
    Player->SetActorLocation(FVector(900,0,200));
    Advance(Fixture, .1f);
    TestFalse(TEXT("Starting outside radius is safe"), Player->IsInFatalTrap());
    Player->SetActorLocation(FVector(300,0,200));
    Advance(Fixture, .1f);
    TestTrue(TEXT("Fresh entry captures player"), Player->IsInFatalTrap());
    Actor->Trap->CancelTrap();
    TestFalse(TEXT("Interruption releases player"), Player->IsInFatalTrap());
    Advance(Fixture, 3.2f);
    TestTrue(TEXT("Cooldown rearms"), Actor->Trap->State == EQuasicomboLarvaeTrapState::Armed);
    TestFalse(TEXT("Staying inside cannot retrigger"), Player->IsInFatalTrap());
    Player->SetActorLocation(FVector(900,0,200)); Advance(Fixture, .1f);
    Player->SetActorLocation(FVector(300,0,200)); Advance(Fixture, .1f);
    TestTrue(TEXT("Exit and reentry trigger the rearmed trap"), Player->IsInFatalTrap());
    Actor->Destroy();
    TestFalse(TEXT("Teardown cannot leave input locked"), Player->IsInFatalTrap());
    Fixture.ForwardErrorMessages(this);
    return !Fixture.HasFailed();
}
#endif
