#if WITH_DEV_AUTOMATION_TESTS
#include "QuasicomboQTEDirector.h"
#include "QuasicomboQTEComponent.h"
#include "QuasicomboQTEPresentation.h"
#include "QuasicomboBoss.h"
#include "QuasicomboEvolutionComponent.h"
#include "QuasicomboEncounterLightingComponent.h"
#include "QuasicomboMusicComponent.h"
#include "SideScrollingGameMode.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "QuasicomboRunSubsystem.h"
#include "QuasicomboVictoryPickup.h"
#include "SideScrollingCharacter.h"
#include "BusterCannonComponent.h"
#include "Tests/AutomationCommon.h"
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Particles/ParticleSystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "QuasicomboHUDActor.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTEJumpUppercutTest,"Quasicombo.QTE.JumpThenUppercut",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FQuasicomboQTEJumpUppercutTest::RunTest(const FString&)
{
    for(bool bTau : {false,true})
    {
        FTestWorldWrapper Fixture;
        if(!Fixture.CreateTestWorld(EWorldType::Game)||!Fixture.BeginPlayInTestWorld()) return false;
        UWorld* World=Fixture.GetTestWorld();
        const float Facing=bTau?-1.f:1.f;
        auto* Mike=World->SpawnActor<ASideScrollingCharacter>(LoadClass<ASideScrollingCharacter>(nullptr,
            TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C")),FVector(-Facing*500,0,200),FRotator::ZeroRotator);
        auto* Boss=World->SpawnActor<AQuasicomboBoss>(LoadClass<AQuasicomboBoss>(nullptr,
            TEXT("/Game/Quasicombo/Boss/BP_QC_Boss.BP_QC_Boss_C")),FVector(0,0,200),FRotator::ZeroRotator);
        if(!Mike||!Boss) return false;
        Boss->Phase=EQuasicomboBossPhase::QTE;
        auto* PC=World->SpawnActor<APlayerController>();
        PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Mike); PC->SetViewTarget(Mike);
        Mike->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Boss->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        auto* D=World->SpawnActor<AQuasicomboQTEDirector>();
        if(!TestTrue(TEXT("Opening starts"),D->Start(Boss,Mike,bTau,1,1,2))) return false;
        auto* Anim=Mike->GetMesh()->GetAnimInstance();
        if(!TestNotNull(TEXT("Mike animation instance"),Anim)||!TestNotNull(TEXT("Crouch montage"),D->MikeMontage.Get())) return false;
        auto Advance=[&](float Seconds)
        {
            while(D->IsActive()&&Seconds>0)
            {
                const float Step=FMath::Min(.02f,Seconds);
                D->HitStopUntil=0; D->LastClock=FPlatformTime::Seconds()-Step;
                Fixture.TickTestWorld(Step*UGameplayStatics::GetGlobalTimeDilation(World));
                Seconds-=Step;
            }
        };
        const FVector BeforeApproach=Mike->GetActorLocation();
        Advance(.4f);
        TestFalse(TEXT("Approach does not rush straight into the first prompt"),D->QTE->IsPromptOpen());
        TestTrue(TEXT("Approach speed stays within its configured limit"),FVector::Distance(Mike->GetActorLocation(),BeforeApproach)<=D->Data->ApproachSpeed*.4f+1);
        for(int32 I=0;I<1000&&!D->QTE->IsPromptOpen();++I) Advance(.02f);
        Advance(.2f);
        TestEqual(TEXT("First Xbox prompt is A / Jump"),D->QTE->GetExpectedInput(),EQuasicomboQTEInput::Jump);
        TestTrue(TEXT("Anticipation holds its final squat pose"),FMath::IsNearlyEqual(
            Anim->Montage_GetPosition(D->MikeMontage.Get()),D->MikeMontage->GetPlayLength()*D->HeldPoseFraction,.002f));
        auto* Instance=Anim->GetActiveInstanceForMontage(D->MikeMontage.Get());
        TestTrue(TEXT("Live squat retains full montage weight"),Instance&&Instance->GetWeight()>.99f);
        D->Submit(EQuasicomboQTEInput::Jump);
        Advance(.3f);
        TestTrue(TEXT("A starts takeoff"),Mike->GetActorLocation().Z>D->MikeGroundZ);
        TestEqual(TEXT("Jump counts once toward combo"),Mike->GetHitComboCount(),1);
        D->Submit(EQuasicomboQTEInput::Attack);
        TestEqual(TEXT("B during takeoff cannot skip to the attack"),Mike->GetHitComboCount(),1);
        Advance(.65f);
        TestEqual(TEXT("Second Xbox prompt is B / Attack"),D->QTE->GetExpectedInput(),EQuasicomboQTEInput::Attack);
        TestTrue(TEXT("Jump has no impact effect"),D->ImpactFX.IsEmpty());
        TestFalse(TEXT("Jump never produces contact"),D->bContact);
        auto* LizardClip=D->Data->Beats[1].Lizard.Get();
        FTransform ContactPose,PromptPose;
        const auto BoneIndex=FSkeletonPoseBoneIndex(LizardClip->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(TEXT("spine_02")));
        LizardClip->GetBoneTransform(ContactPose,BoneIndex,FAnimExtractContext(LizardClip->GetPlayLength()*D->Data->Beats[1].ContactFraction,false),false);
        LizardClip->GetBoneTransform(PromptPose,BoneIndex,FAnimExtractContext(LizardClip->GetPlayLength()*D->HeldPoseFraction,false),false);
        TestTrue(TEXT("Lizard source holds its pre-reaction pose through anticipation"),PromptPose.Equals(ContactPose,.035f));
        const FQuat VisibleLizard=Boss->GetMesh()->GetBoneSpaceTransforms()[Boss->GetMesh()->GetBoneIndex(TEXT("spine_02"))].GetRotation();
        TestTrue(FString::Printf(TEXT("Jump never starts a lizard hit reaction; visible %s expected %s cursor %.3f"),*VisibleLizard.ToString(),*PromptPose.GetRotation().ToString(),Boss->GetMesh()->GetAnimInstance()->Montage_GetPosition(D->BossMontage.Get())),VisibleLizard.Equals(PromptPose.GetRotation(),.035f));
        TestEqual(TEXT("Jump leaves lizard in place"),Boss->GetActorLocation(),D->BossStart.GetLocation());
        TestEqual(TEXT("Jump has no hit stop"),D->HitStopUntil,0.0);
        const FVector Airborne=Mike->GetActorLocation();
        TestTrue(TEXT("Mike remains airborne for the uppercut prompt"),Airborne.Z>=D->MikeGroundZ+69);
        const auto HeldUppercut=D->MikeMontage;
        const float UppercutPosition=Anim->Montage_GetPosition(HeldUppercut.Get());
        TestTrue(TEXT("B pauses early within the uppercut"),UppercutPosition>0&&UppercutPosition<HeldUppercut->GetPlayLength()*.3f);
        Advance(.3f);
        TestEqual(TEXT("Waiting for B holds altitude and position"),Mike->GetActorLocation(),Airborne);
        TestEqual(TEXT("Waiting for B holds the uppercut animation cursor"),Anim->Montage_GetPosition(HeldUppercut.Get()),UppercutPosition);
        D->Submit(EQuasicomboQTEInput::Attack);
        TestTrue(TEXT("B continues the held uppercut instead of restarting it"),D->MikeMontage==HeldUppercut);
        TestEqual(TEXT("Uppercut counts exactly once"),Mike->GetHitComboCount(),2);
        Advance(.2f);
        TestTrue(TEXT("Uppercut anticipation precedes contact"),D->ImpactFX.IsEmpty());
        Advance(.35f);
        TestTrue(TEXT("B produces the first attack contact"),D->bContact);
        TestTrue(FString::Printf(TEXT("Uppercut advances the paired reaction past Contact; cursor %.3f threshold %.3f stage %d"),Boss->GetMesh()->GetAnimInstance()->Montage_GetPosition(D->BossMontage.Get()),LizardClip->GetPlayLength()*D->Data->Beats[1].ContactFraction,int(D->Stage)),Boss->GetMesh()->GetAnimInstance()->Montage_GetPosition(D->BossMontage.Get())+.002f>=LizardClip->GetPlayLength()*D->Data->Beats[1].ContactFraction);
        TestEqual(TEXT("Uppercut contact produces one impact"),D->ImpactFX.Num(),1);
        TestEqual(TEXT("Contact adds no extra combo"),Mike->GetHitComboCount(),2);
        Advance(.55f);
        TestTrue(TEXT("Uppercut ends airborne before deliberate recovery"),Mike->GetActorLocation().Z>D->MikeGroundZ+30);
        TestEqual(TEXT("Two prompts end on uppercut without another attack"),D->Beat,2);
        Advance(1.2f);
        TestTrue(TEXT("Two-prompt recovery lands before returning control"),FMath::IsNearlyEqual(Mike->GetActorLocation().Z,D->MikeGroundZ,.1f));
        D->Destroy();
        Fixture.ForwardErrorMessages(this);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTEFailureFeedbackTest,"Quasicombo.QTE.FailureFeedbackBeforeCounter",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FQuasicomboQTEFailureFeedbackTest::RunTest(const FString&)
{
    // A, B, RT, X, wrong input while charging, early release, timeout and Y.
    for(int32 Case=0;Case<8;++Case)
    {
        FTestWorldWrapper Fixture;
        if(!Fixture.CreateTestWorld(EWorldType::Game)||!Fixture.BeginPlayInTestWorld()) return false;
        auto* World=Fixture.GetTestWorld();
        auto* Mike=World->SpawnActor<ASideScrollingCharacter>(LoadClass<ASideScrollingCharacter>(nullptr,
            TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C")),FVector(Case%2?500:-500,0,200),FRotator::ZeroRotator);
        auto* Boss=World->SpawnActor<AQuasicomboBoss>(LoadClass<AQuasicomboBoss>(nullptr,
            TEXT("/Game/Quasicombo/Boss/BP_QC_Boss.BP_QC_Boss_C")),FVector(0,0,200),FRotator::ZeroRotator);
        if(!Mike||!Boss) return false;
        Boss->Quantum->InitializePreview(0.f);
        Boss->Phase=EQuasicomboBossPhase::QTE;
        Mike->GetBusterCannon()->Equip();
        auto* PC=World->SpawnActor<APlayerController>();
        PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Mike); PC->SetViewTarget(Mike);
        Mike->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Boss->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        auto* D=World->SpawnActor<AQuasicomboQTEDirector>();
        if(!D->Start(Boss,Mike,Case==2,1,2,6)) return false;
        auto* HUD=CreateWidget<UQuasicomboHUDWidget>(PC,UQuasicomboHUDWidget::StaticClass());
        if(!HUD) return false;
        // The fixture ticks GC while this widget is outside any viewport.
        TStrongObjectPtr<UQuasicomboHUDWidget> HUDLifetime(HUD);
        HUD->TakeWidget(); HUD->NativeConstruct();
        const int32 Target=Case==0?1:Case==2?4:Case==7?3:Case>=3&&Case<=5?6:2;
        for(int32 Beat=1;Beat<Target;++Beat)
        {
            D->AdvanceToPromptForTesting(); D->Submit(D->QTE->GetExpectedInput());
            D->AdvanceForTesting(2.2f);
        }
        D->AdvanceToPromptForTesting();
        const auto Expected=D->QTE->GetExpectedInput();
        if(Case==4||Case==5) D->Submit(EQuasicomboQTEInput::Fire);
        auto Advance=[&](float Seconds)
        {
            while(D->IsActive()&&Seconds>0)
            {
                const float Step=FMath::Min(.02f,Seconds);
                D->HitStopUntil=0; D->LastClock=FPlatformTime::Seconds()-Step;
                Fixture.TickTestWorld(Step*UGameplayStatics::GetGlobalTimeDilation(World));
                Seconds-=Step;
            }
        };
        Advance(.08f);
        const FVector Held=Mike->GetActorLocation();
        const FVector HeldBoss=Boss->GetMesh()->GetSocketLocation(TEXT("head"));
        const FVector HeldCamera=D->Camera->GetActorLocation();
        auto* Anim=Mike->GetMesh()->GetAnimInstance();
        const auto Montage=D->MikeMontage;
        const float Cursor=Anim->Montage_GetPosition(Montage.Get());
        const float HP=Mike->GetCurrentHP();
        if(Case==6)
        {
            D->QTE->Sequence.OpenNextPrompt(FPlatformTime::Seconds()-2);
            D->QTE->TickComponent(.02f,LEVELTICK_All,nullptr);
        }
        else if(Case==5) D->Submit(EQuasicomboQTEInput::Fire,EQuasicomboQTEInputEdge::Release);
        else D->Submit(Expected==EQuasicomboQTEInput::Jump?EQuasicomboQTEInput::Attack:EQuasicomboQTEInput::Jump);
        TestTrue(TEXT("Failure shows feedback before recovery or counter"),D->IsShowingFailureFeedback());
        TestFalse(TEXT("Failure closes input immediately"),D->QTE->IsPromptOpen());
        TestEqual(TEXT("Feedback remembers the expected input, not the wrong press"),D->QTE->GetFailedInput(),Expected);
        HUD->NativeTick(FGeometry(),.02f);
        auto* Icon=Cast<UBorder>(HUD->GetWidgetFromName(TEXT("PromptIcon")));
        auto* Glyph=Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("PromptIconText")));
        auto* Label=Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("PromptText")));
        if(!TestNotNull(TEXT("Failure icon"),Icon)||!TestNotNull(TEXT("Failure glyph"),Glyph)||!TestNotNull(TEXT("Failure label"),Label)) return false;
        const FString ExpectedGlyph=Expected==EQuasicomboQTEInput::Jump?TEXT("A"):Expected==EQuasicomboQTEInput::Attack?TEXT("B"):Expected==EQuasicomboQTEInput::Chomp?TEXT("Y"):Expected==EQuasicomboQTEInput::Claw?TEXT("RT"):TEXT("X");
        TestEqual(TEXT("Failed prompt still shows the correct button"),Glyph->GetText().ToString(),ExpectedGlyph);
        TestEqual(TEXT("Expected button flashes bright red"),Icon->GetBrushColor(),FLinearColor(1.f,.015f,.01f));
        TestEqual(TEXT("Miss is explicit even when B is normally red"),Label->GetText().ToString(),FString(TEXT("Miss")));
        TestEqual(TEXT("Failure icon remains visible"),Icon->GetVisibility(),ESlateVisibility::HitTestInvisible);
        D->Submit(Expected);
        Advance(.2f);
        TestTrue(TEXT("Feedback remains visible for its brief hold"),D->IsShowingFailureFeedback());
        TestEqual(TEXT("No accepted combo during failure feedback"),Mike->GetHitComboCount(),0);
        TestEqual(TEXT("Failure hold freezes Mike placement"),Mike->GetActorLocation(),Held);
        TestTrue(TEXT("Failure hold freezes lizard pose including launched ragdoll"),Boss->GetMesh()->GetSocketLocation(TEXT("head")).Equals(HeldBoss,.1f));
        TestEqual(TEXT("Failure hold freezes animation cursor"),Anim->Montage_GetPosition(Montage.Get()),Cursor);
        TestEqual(TEXT("Failure hold keeps the current camera"),D->Camera->GetActorLocation(),HeldCamera);
        TestEqual(TEXT("Counter damage waits for feedback"),Mike->GetCurrentHP(),HP);
        // Standalone fixture controllers have no logged-in PlayerState. Supply
        // the engine's world pause owner explicitly instead of a silent no-op.
        World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
        TestTrue(TEXT("Fixture enters actual game pause"),World->IsPaused());
        D->AdvanceForTesting(1.f);
        TestTrue(TEXT("Game pause preserves the failure flash"),D->IsShowingFailureFeedback());
        World->GetWorldSettings()->SetPauserPlayerState(nullptr);
        if(Case==0)
        {
            D->Destroy();
            TestFalse(TEXT("Interruption releases input during the failure flash"),D->QTE->IsQTEActive());
            TestEqual(TEXT("Interruption clears the failed prompt"),D->QTE->GetFailedInput(),EQuasicomboQTEInput::Invalid);
            TestFalse(TEXT("Interruption releases combat lock"),World->GetSubsystem<UQuasicomboRunSubsystem>()->IsBossCombatLocked());
            Fixture.ForwardErrorMessages(this);
            continue;
        }
        Advance(.25f);
        TestEqual(TEXT("Counter follows the flash"),D->Stage,AQuasicomboQTEDirector::EStage::Counter);
        HUD->NativeTick(FGeometry(),.02f);
        TestEqual(TEXT("Failure glyph hides for the counter"),Icon->GetVisibility(),ESlateVisibility::Collapsed);
        Advance(3.f);
        TestFalse(TEXT("Counter completes and restores combat"),D->IsActive());
        TestTrue(TEXT("Counter applies damage after the flash"),Mike->GetCurrentHP()<HP);
        Fixture.ForwardErrorMessages(this);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTEPromptPoseHoldTest,"Quasicombo.QTE.LivePromptPoseHolds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FQuasicomboQTEPromptPoseHoldTest::RunTest(const FString&)
{
    FTestWorldWrapper Fixture;
    if(!Fixture.CreateTestWorld(EWorldType::Game)||!Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World=Fixture.GetTestWorld();
    auto* Mike=World->SpawnActor<ASideScrollingCharacter>(LoadClass<ASideScrollingCharacter>(nullptr,
        TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C")),FVector(-500,0,200),FRotator::ZeroRotator);
    auto* Boss=World->SpawnActor<AQuasicomboBoss>(LoadClass<AQuasicomboBoss>(nullptr,
        TEXT("/Game/Quasicombo/Boss/BP_QC_Boss.BP_QC_Boss_C")),FVector(0,0,200),FRotator::ZeroRotator);
    if(!Mike||!Boss) return false;
    Boss->Phase=EQuasicomboBossPhase::QTE;
    auto* PC=World->SpawnActor<APlayerController>();
    PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Mike); PC->SetViewTarget(Mike);
    auto* Mesh=Mike->GetMesh();
    Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    auto* D=World->SpawnActor<AQuasicomboQTEDirector>();
    if(!D->Start(Boss,Mike,false,1,1,5)) return false;
    auto Advance=[&](float Seconds)
    {
        while(D->IsActive()&&Seconds>0)
        {
            const float Step=FMath::Min(.02f,Seconds);
            D->HitStopUntil=0; D->LastClock=FPlatformTime::Seconds()-Step;
            Fixture.TickTestWorld(Step*UGameplayStatics::GetGlobalTimeDilation(World));
            Seconds-=Step;
        }
    };
    for(int32 Number=1;Number<=5;++Number)
    {
        for(int32 I=0;I<1000&&D->IsActive()&&!D->QTE->IsPromptOpen();++I) Advance(.02f);
        if(!TestTrue(TEXT("Each move reaches a protected prompt"),D->IsActive()&&D->QTE->IsPromptOpen())) return false;
        Advance(.2f);
        auto* Anim=Mesh->GetAnimInstance();
        auto* Montage=D->MikeMontage.Get();
        auto* Instance=Anim->GetActiveInstanceForMontage(Montage);
        TestTrue(TEXT("Prompt keeps its live montage fully weighted"),Instance&&Instance->GetWeight()>.99f);
        TestFalse(TEXT("Prompt pauses playback"),Anim->Montage_IsPlaying(Montage));
        const float Position=Anim->Montage_GetPosition(Montage);
        TestTrue(TEXT("Prompt is inside the clip, before it ends"),Position>0&&Position<Montage->GetPlayLength());
        UAnimSequence* Clip=Number==1?D->Data->MikeJumpReady.Get():D->Data->Beats[Number-1].Mike.Get();
        for(FName Bone : {FName(TEXT("calf_r")),FName(TEXT("upperarm_r"))})
        {
            FTransform Expected;
            Clip->GetBoneTransform(Expected,FSkeletonPoseBoneIndex(Clip->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(Bone)),FAnimExtractContext(Position,false),false);
            const FTransform Visible=Mesh->GetBoneSpaceTransforms()[Mesh->GetBoneIndex(Bone)];
            TestTrue(FString::Printf(TEXT("Prompt %d holds the evaluated %s pose instead of idle"),Number,*Bone.ToString()),Visible.GetRotation().Equals(Expected.GetRotation(),.025f));
        }
        const FTransform HeldBone=Mesh->GetBoneSpaceTransforms()[Mesh->GetBoneIndex(TEXT("calf_r"))];
        const FVector HeldLocation=Mike->GetActorLocation();
        Advance(.7f);
        Instance=Anim->GetActiveInstanceForMontage(Montage);
        TestTrue(TEXT("Waiting cannot fade the held pose to locomotion"),Instance&&Instance->GetWeight()>.99f);
        TestEqual(TEXT("Waiting cannot advance the animation cursor"),Anim->Montage_GetPosition(Montage),Position);
        TestTrue(TEXT("Visible skeleton remains at the held pose"),Mesh->GetBoneSpaceTransforms()[Mesh->GetBoneIndex(TEXT("calf_r"))].Equals(HeldBone,.025f));
        TestEqual(TEXT("Waiting cannot move Mike"),Mike->GetActorLocation(),HeldLocation);
        TestFalse(TEXT("Anticipation cannot produce contact"),D->bContact);
        if(Number==5) break;
        D->Submit(D->QTE->GetExpectedInput());
        if(Number>1) TestTrue(TEXT("Accepted input continues the same live montage"),D->MikeMontage.Get()==Montage);
        TestEqual(TEXT("One combo increment per accepted input"),Mike->GetHitComboCount(),Number);
        Advance(.02f); // Leave this prompt before waiting for the next one.
    }
    D->Destroy();
    Fixture.ForwardErrorMessages(this);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTEDirectorTest,"Quasicombo.QTE.CombatRoundsComboAndObliteration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FQuasicomboQTEDirectorTest::RunTest(const FString&)
{
    FTestWorldWrapper Fixture;
    if(!Fixture.CreateTestWorld(EWorldType::Game)||!Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World=Fixture.GetTestWorld();
    auto* Run=World->GetSubsystem<UQuasicomboRunSubsystem>();
    auto* Sun=World->SpawnActor<ADirectionalLight>()->GetLightComponent();
    Sun->SetMobility(EComponentMobility::Movable); Sun->SetIntensity(20.f);
    UClass* MikeClass=LoadClass<ASideScrollingCharacter>(nullptr,TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
    UClass* BossClass=LoadClass<AQuasicomboBoss>(nullptr,TEXT("/Game/Quasicombo/Boss/BP_QC_Boss.BP_QC_Boss_C"));
    auto* Mike=World->SpawnActor<ASideScrollingCharacter>(MikeClass,FVector(-500,0,200),FRotator::ZeroRotator);
    auto* Boss=World->SpawnActor<AQuasicomboBoss>(BossClass,FVector(0,0,200),FRotator::ZeroRotator);
    if(!TestNotNull(TEXT("Mike"),Mike)||!TestNotNull(TEXT("Lizard"),Boss)) return false;
    auto* PC=World->SpawnActor<APlayerController>();
    PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    PC->Possess(Mike); PC->SetViewTarget(Mike);
    Boss->Quantum->bUseLiveQuantumApi=false;
    Boss->EvolutionMinimumSeconds=Boss->EvolutionTimeoutSeconds=0;
    Boss->Evolution->DurationScale=.001f;
    Boss->EncounterLighting->CombatFadeSeconds=Boss->EncounterLighting->EvolutionFadeSeconds=0;
    Boss->FirstArmorThreshold=Boss->SecondArmorThreshold=0;
    for(int32 N=1;N<=3;++N) Run->TryCommitRoute(N,EQuasicomboLane::A);
    Boss->StartEncounter(Mike);
    TestEqual(TEXT("Rounds remain hidden until the finisher is sampled"),Boss->GetAdditionalQTERoundsRemaining(),0);
    auto* HUD=CreateWidget<UQuasicomboHUDWidget>(PC);
    if(!TestNotNull(TEXT("Rounds HUD"),HUD)) return false;
    const auto SlateHUD=HUD->TakeWidget(); HUD->NativeConstruct();
    auto* RoundsText=Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("RoundsRemainingText")));
    if(!TestNotNull(TEXT("Independent rounds counter"),RoundsText)) return false;
    auto CheckRoundsHUD=[&](int32 Additional)
    {
        HUD->NativeTick(FGeometry(),.02f);
        TestEqual(TEXT("Rounds HUD displays the additional-round count"),RoundsText->GetText().ToString(),
            Additional>0?FString::Printf(TEXT("Rounds left: %d"),Additional):FString());
        TestEqual(TEXT("Zero collapses the rounds counter"),RoundsText->GetVisibility(),
            Additional>0?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    };
    CheckRoundsHUD(0);
    Boss->bOutcomeSampled=true;
    for(int32 Rounds=1;Rounds<=3;++Rounds)
    {
        Boss->FinisherRounds=Rounds;
        for(int32 Round=1;Round<=Rounds;++Round)
        {
            Boss->FinisherRound=Round;
            TestEqual(FString::Printf(TEXT("Round %d of %d displays only additional rounds"),Round,Rounds),
                Boss->GetAdditionalQTERoundsRemaining(),Rounds-Round);
            CheckRoundsHUD(Rounds-Round);
        }
    }
    Boss->FinisherRounds=3; Boss->FinisherRound=1;
    TestTrue(TEXT("Boss failure enters its retry pause"),Boss->HandleEncounterFailure());
    TestEqual(TEXT("Retry preserves the additional-round count"),Boss->GetAdditionalQTERoundsRemaining(),2);
    CheckRoundsHUD(2);
    Boss->RestartEncounter(); Boss->Failures=0;
    TestEqual(TEXT("Resumed combat preserves the additional-round count"),Boss->GetAdditionalQTERoundsRemaining(),2);
    Boss->Phase=EQuasicomboBossPhase::AwaitPickup;
    CheckRoundsHUD(0);
    Boss->Phase=EQuasicomboBossPhase::Complete;
    CheckRoundsHUD(0);
    Boss->Phase=EQuasicomboBossPhase::Combat;
    Boss->bOutcomeSampled=false; Boss->FinisherRound=Boss->FinisherRounds=1;
    for(int32 I=0;I<Boss->BaseHealthHits-Boss->EvolutionTriggerHealth;++I)
        Boss->ApplyDamage(1,Mike,Boss->GetActorLocation(),FVector::ZeroVector);
    for(int32 I=0;I<12;++I) Fixture.TickTestWorld(.02f);
    Boss->bOutcomeSampled=true; Boss->bTauOutcome=false;
    Boss->FinalPromptCount=6; Boss->FinisherRounds=3; Boss->FinisherRound=1;
    Mike->GetBusterCannon()->Equip(); Mike->RegisterHitCombo(); Mike->RegisterHitCombo();
    auto Deplete=[&]()
    {
        for(int32 I=0;I<Boss->BaseHealthHits+2*Boss->HitsPerArmorBar&&Boss->GetBossPhase()==EQuasicomboBossPhase::Combat;++I)
            Boss->ApplyDamage(1,Mike,Boss->GetActorLocation(),FVector::ZeroVector);
        auto* D=Run->GetQTEDirector(); if(D) D->AdvanceToPromptForTesting(); return D;
    };
    auto PressRound=[&](AQuasicomboQTEDirector* D,int32 Count)
    {
        for(int32 I=0;I<Count;++I)
        {
            
            const int32 Combo=Mike->GetHitComboCount();
            D->Submit(D->QTE->GetExpectedInput());
            TestEqual(TEXT("Accepted press adds one combo"),Mike->GetHitComboCount(),Combo+1);
            D->Submit(EQuasicomboQTEInput::Invalid);
            TestEqual(TEXT("Input during action cannot skip or fail"),Mike->GetHitComboCount(),Combo+1);
            
            D->AdvanceForTesting(2.2f);
            
        }
    };
    
    auto* D=Deplete(); if(!TestNotNull(TEXT("First director"),D)) return false;
    TestEqual(TEXT("First round is four"),D->QTE->GetPromptCount(),4);
    TestEqual(TEXT("First QTE shows two additional rounds"),Boss->GetAdditionalQTERoundsRemaining(),2);
    CheckRoundsHUD(2);
    TestEqual(TEXT("QTE hides the boss panel while leaving rounds visible"),
        Cast<UBorder>(HUD->GetWidgetFromName(TEXT("BossInfoPanel")))->GetVisibility(),ESlateVisibility::Collapsed);
    auto* Music=Cast<ASideScrollingGameMode>(UGameplayStatics::GetGameMode(World))->MusicController.Get();
    Music->AdvanceCrossfade(1.f);
    TestEqual(TEXT("Boss entry requests QTE music ducking"),Music->State,EQuasicomboMusicState::QTE);
    TestTrue(TEXT("Live QTE music reaches forty percent"),FMath::IsNearlyEqual(Music->GetOutputVolume(Music->CurrentMusic),Music->EncounterVolume*.4f,.0001f));
    for(int32 I=0;I<60;++I) Fixture.TickTestWorld(.1f);
    TestEqual(TEXT("Combo survives beyond normal expiry during QTE"),Mike->GetHitComboCount(),2);
    const float HP=Mike->GetCurrentHP();
    
    PressRound(D,4); D->AdvanceForTesting(2);
    TestEqual(TEXT("Successful nonfinal counter halves current health"),Mike->GetCurrentHP(),HP*.5f);
    TestEqual(TEXT("Successful counter preserves combo"),Mike->GetHitComboCount(),6);
    TestEqual(TEXT("Full base health restored"),Boss->GetBaseHealthFraction(),1.f);
    TestEqual(TEXT("Both awarded shields restored"),Boss->GetArmorBars(),2);
    TestEqual(TEXT("Combat strike count reset"),Boss->GetAcceptedStrikes(),0);
    TestEqual(TEXT("Nonfinal success displays one additional round during combat"),Boss->GetAdditionalQTERoundsRemaining(),1);
    CheckRoundsHUD(1);
    Music->AdvanceCrossfade(.25f);
    TestEqual(TEXT("Counter restores combat music state"),Music->State,EQuasicomboMusicState::Combat);
    TestTrue(TEXT("Counter restores normal music volume"),FMath::IsNearlyEqual(Music->GetOutputVolume(Music->CurrentMusic),Music->EncounterVolume,.0001f));
    
    D=Deplete(); if(!D) return false;
    TestEqual(TEXT("Second round is five"),D->QTE->GetPromptCount(),5);
    TestEqual(TEXT("Second QTE keeps one additional round visible"),Boss->GetAdditionalQTERoundsRemaining(),1);
    D->Submit(EQuasicomboQTEInput::Attack);
    for(int32 I=0;I<100&&D->IsActive();++I) D->AdvanceForTesting(.05f);
    TestFalse(TEXT("Failed round completes its feedback and counter before retry"),D->IsActive());
    TestEqual(TEXT("Wrong button clears combo"),Mike->GetHitComboCount(),0);
    TestEqual(TEXT("Failure halves current health again"),Mike->GetCurrentHP(),HP*.25f);
    TestEqual(TEXT("Failed attempt preserves additional rounds"),Boss->GetAdditionalQTERoundsRemaining(),1);
    CheckRoundsHUD(1);
    
    D=Deplete(); if(!D) return false;
    TestEqual(TEXT("Failed round repeats same count"),D->QTE->GetPromptCount(),5);
    TestEqual(TEXT("Failed round repeats button family"),D->QTE->GetExpectedInput(),EQuasicomboQTEInput::Jump);
    Music->AdvanceCrossfade(.25f);
    const int32 StartsBeforeInterrupt=Music->TrackStarts;
    D->Destroy();
    Music->AdvanceCrossfade(.25f);
    TestEqual(TEXT("Interrupted QTE restores combat music state"),Music->State,EQuasicomboMusicState::Combat);
    TestTrue(TEXT("Interrupted QTE restores normal volume"),FMath::IsNearlyEqual(Music->GetOutputVolume(Music->CurrentMusic),Music->EncounterVolume,.0001f));
    TestEqual(TEXT("Music ducking and restoration preserve the song"),Music->TrackStarts,StartsBeforeInterrupt);
    TestEqual(TEXT("Interruption preserves additional rounds"),Boss->GetAdditionalQTERoundsRemaining(),1);
    D=Deplete(); if(!D) return false;
    
    PressRound(D,5); D->AdvanceForTesting(2);
    
    D=Deplete(); if(!D) return false;
    TestEqual(TEXT("Final round is six"),D->QTE->GetPromptCount(),6);
    TestEqual(TEXT("Final round hides additional rounds"),Boss->GetAdditionalQTERoundsRemaining(),0);
    CheckRoundsHUD(0);
    
    PressRound(D,5);
    D->AdvanceToPromptForTesting();
    TestTrue(TEXT("Cannon prompt follows deliberate landing"),FMath::IsNearlyEqual(Mike->GetActorLocation().Z,D->MikeGroundZ,.1f));
    TestEqual(TEXT("Sixth beat uses cannon"),D->QTE->GetExpectedInput(),EQuasicomboQTEInput::Fire);
    const int32 BeforeCharge=Mike->GetHitComboCount();
    // Inject elapsed real time for charging without sleeping the automation runner.
    
    D->QTE->Sequence.Submit(EQuasicomboQTEInput::Fire,FPlatformTime::Seconds()-1.1,false);
    D->QTE->OnPromptAccepted.Broadcast(6,true);
    TestEqual(TEXT("Hold start counts once"),Mike->GetHitComboCount(),BeforeCharge+1);
    D->Submit(EQuasicomboQTEInput::Fire);
    D->Submit(EQuasicomboQTEInput::Fire,EQuasicomboQTEInputEdge::Release);
    TestEqual(TEXT("Held repeat and release add no combo"),Mike->GetHitComboCount(),BeforeCharge+1);
    TestNotNull(TEXT("Cinematic projectile exists"),D->ShotFX.Get());
    if(D->ShotFX) TestTrue(TEXT("Cinematic visual is three times enlarged normal shot"),FMath::IsNearlyEqual(D->ShotFX->GetComponentScale().X,342.f/Mike->GetBusterCannon()->ThunderVisualDiameterCm));
    
    D->AdvanceForTesting(D->Data->FindSegment(TEXT("CannonFire"))->Duration+.25f);
    TestTrue(TEXT("Boss removed under explosion"),Boss->IsActorBeingDestroyed());
    TestTrue(TEXT("Director retains controls after boss destruction"),Run->IsBossCombatLocked());
    TestTrue(TEXT("Director retains camera after boss destruction"),PC->GetViewTarget()==D->Camera);
    TestEqual(TEXT("Obliteration retains one-lux reward lighting"),Sun->Intensity,1.f);
    TestEqual(TEXT("Explosion cannot end the level before its outro"),Run->GetOutcome(),EQuasicomboRunOutcome::Playing);
    D->AdvanceForTesting(1.5f);
    TestFalse(TEXT("Controls restored after explosion"),Run->IsBossCombatLocked());
    TestEqual(TEXT("Global time restored"),UGameplayStatics::GetGlobalTimeDilation(World),1.f);
    TestTrue(TEXT("Gameplay camera restored"),PC->GetViewTarget()==Mike);
    TestEqual(TEXT("Reward retains one-lux sunlight"),Sun->Intensity,1.f);
    auto* Mode=Cast<ASideScrollingGameMode>(UGameplayStatics::GetGameMode(World));
    if(TestNotNull(TEXT("Encounter music owner"),Mode))
        TestEqual(TEXT("Sixth-beat victory starts music fade after boss removal"),Mode->MusicController->State,EQuasicomboMusicState::Defeated);
    auto* Reward=UGameplayStatics::GetActorOfClass(World,AQuasicomboVictoryPickup::StaticClass());
    TestNull(TEXT("Equipped cannon skips the sixth-beat reward"),Reward);
    TestEqual(TEXT("Equipped victory completes after the explosion outro"),Run->GetOutcome(),EQuasicomboRunOutcome::Victory);
    TestTrue(TEXT("Equipped victory screen is ready after cleanup"),Run->IsVictoryPresentationReady());
    TestEqual(TEXT("Equipped victory does not replay acquisition"),Mike->GetBusterCannon()->AcquisitionPhase,EBusterAcquisitionPhase::None);
    Fixture.ForwardErrorMessages(this);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTEPresentationInterruptionTest,"Quasicombo.QTE.PresentationInterruption",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FQuasicomboQTEPresentationInterruptionTest::RunTest(const FString&)
{
    FTestWorldWrapper Fixture;
    if(!Fixture.CreateTestWorld(EWorldType::Game)||!Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World=Fixture.GetTestWorld();
    auto* Run=World->GetSubsystem<UQuasicomboRunSubsystem>();
    auto* Mike=World->SpawnActor<ASideScrollingCharacter>(LoadClass<ASideScrollingCharacter>(nullptr,
        TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C")),FVector(-500,0,200),FRotator::ZeroRotator);
    auto* Boss=World->SpawnActor<AQuasicomboBoss>();
    if(!Mike||!Boss) return false;
    auto* PC=World->SpawnActor<APlayerController>(); PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Mike); PC->SetViewTarget(Mike);
    Mike->RegisterHitCombo(); Mike->RegisterHitCombo();
    const FTransform MikeStart=Mike->GetActorTransform(),BossStart=Boss->GetActorTransform();
    const EMovementMode Movement=Mike->GetCharacterMovement()->MovementMode;
    UGameplayStatics::SetGlobalTimeDilation(World,.8f);
    auto* D=World->SpawnActor<AQuasicomboQTEDirector>();
    if(!TestTrue(TEXT("Presentation started"),D->Start(Boss,Mike,false,1,1,5))) return false;
    D->AdvanceToPromptForTesting(); D->Submit(EQuasicomboQTEInput::Jump); D->AdvanceForTesting(.2f);
    D->Destroy();
    TestFalse(TEXT("Interruption releases input ownership"),Run->IsBossCombatLocked());
    TestTrue(TEXT("Player transform restored"),Mike->GetActorTransform().Equals(MikeStart));
    TestTrue(TEXT("Boss transform restored"),Boss->GetActorTransform().Equals(BossStart));
    TestTrue(TEXT("Player collision restored"),Mike->GetActorEnableCollision());
    TestTrue(TEXT("Boss collision restored"),Boss->GetActorEnableCollision());
    TestEqual(TEXT("Previous time dilation restored"),UGameplayStatics::GetGlobalTimeDilation(World),.8f);
    TestEqual(TEXT("Player movement restored"),Mike->GetCharacterMovement()->MovementMode,Movement==MOVE_None?MOVE_Walking:Movement);
    TestTrue(TEXT("Gameplay camera restored"),PC->GetViewTarget()==Mike);
    // The restored .8 time dilation also slows the gameplay combo timer.
    const int32 ExpiryTicks=FMath::CeilToInt((Mike->GetHitComboTimeRemaining()+.2f)/(.1f*.8f));
    for(int32 I=0;I<ExpiryTicks;++I) Fixture.TickTestWorld(.1f);
    TestEqual(TEXT("Combo expiry resumes after interruption"),Mike->GetHitComboCount(),0);
    UGameplayStatics::SetGlobalTimeDilation(World,1.f);
    Fixture.ForwardErrorMessages(this);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTEAirborneChainTest,"Quasicombo.QTE.AirborneChainContactAndRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FQuasicomboQTEAirborneChainTest::RunTest(const FString&)
{
    for(float Facing : {1.f,-1.f}) for(int32 Selection : {2,3,4,5,6,7,-5}) for(bool bEquipped : {false,true})
    {
        const int32 Prompts=Selection==7?5:FMath::Abs(Selection);
        if(Prompts==6&&!bEquipped) continue;
        const bool bFail=Selection<0;
        FTestWorldWrapper Fixture;
        if(!Fixture.CreateTestWorld(EWorldType::Game)||!Fixture.BeginPlayInTestWorld()) return false;
        auto* World=Fixture.GetTestWorld();
        auto* Mike=World->SpawnActor<ASideScrollingCharacter>(LoadClass<ASideScrollingCharacter>(nullptr,
            TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C")),FVector(-Facing*500,0,200),FRotator::ZeroRotator);
        auto* Boss=World->SpawnActor<AQuasicomboBoss>(LoadClass<AQuasicomboBoss>(nullptr,
            TEXT("/Game/Quasicombo/Boss/BP_QC_Boss.BP_QC_Boss_C")),FVector(0,0,200),FRotator::ZeroRotator);
        if(!Mike||!Boss) return false;
        auto* Floor=World->SpawnActor<AActor>();
        auto* FloorBox=NewObject<UBoxComponent>(Floor);
        Floor->SetRootComponent(FloorBox);
        FloorBox->SetBoxExtent(FVector(3000,2000,20));
        FloorBox->SetCollisionObjectType(ECC_WorldStatic);
        FloorBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        FloorBox->SetCollisionResponseToAllChannels(ECR_Block);
        Floor->SetActorLocation(FVector(0,0,Boss->GetActorLocation().Z-Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-20));
        FloorBox->SetMobility(EComponentMobility::Static);
        FloorBox->RegisterComponent();
        Boss->Quantum->InitializePreview(0.f);
        Boss->Phase=EQuasicomboBossPhase::QTE;
        auto* PC=World->SpawnActor<APlayerController>(); PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Mike);
        auto* Mesh=Mike->GetMesh(); Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Boss->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        if(bEquipped) Mike->GetBusterCannon()->Equip();
        auto* D=World->SpawnActor<AQuasicomboQTEDirector>();
        // Five is nonfinal here; seven exercises the final five-prompt ending.
        const bool Final=Selection!=5;
        if(!TestTrue(TEXT("Airborne presentation starts"),D->Start(Boss,Mike,Facing<0,1,Final?1:2,Prompts))) return false;
        auto* HUD=CreateWidget<UQuasicomboHUDWidget>(PC);
        if(!TestNotNull(TEXT("QTE HUD can be built"),HUD)) return false;
        const auto SlateHUD=HUD->TakeWidget();
        bool SawApex=false,SawFinisherContact=false,SawDeathPose=false;
        auto Step=[&]()
        {
            D->HitStopUntil=0; D->LastClock=FPlatformTime::Seconds()-.02;
            Fixture.TickTestWorld(.02f*UGameplayStatics::GetGlobalTimeDilation(World));
            if(!D->IsActive()) return;
            if(D->Stage==AQuasicomboQTEDirector::EStage::Apex)
            {
                SawApex=true;
                TestTrue(TEXT("Apex transition stays above ground"),Mike->GetActorLocation().Z>D->MikeGroundZ+30);
                TestEqual(TEXT("Apex adds no combo point"),Mike->GetHitComboCount(),D->Beat);
            }
            if(D->Stage==AQuasicomboQTEDirector::EStage::Action&&D->Beat>=2&&D->Beat<=4)
                TestTrue(TEXT("Uppercut, Chomp and Claw stay airborne"),Mike->GetActorLocation().Z>D->MikeGroundZ+30);
            if(D->Stage==AQuasicomboQTEDirector::EStage::Fall&&Prompts<5)
            {
                SawDeathPose=true;
                TestFalse(TEXT("Short prefixes finish the death clip before ragdoll"),Boss->GetMesh()->IsSimulatingPhysics());
                auto* Instance=Boss->GetMesh()->GetAnimInstance()->GetActiveInstanceForMontage(D->BossMontage.Get());
                if(D->Age>.15f) TestTrue(TEXT("Death montage keeps full weight through its ending"),Instance&&Instance->GetWeight()>.99f);
            }
            if(D->Stage==AQuasicomboQTEDirector::EStage::Action&&D->Beat==5&&D->bContact&&!SawFinisherContact)
            {
                SawFinisherContact=true;
                const FVector Hand=Mesh->GetSocketLocation(TEXT("hand_r"));
                const FVector Target=Boss->GetMesh()->GetSocketLocation(TEXT("head"));
                TestTrue(FString::Printf(TEXT("Evaluated finisher hand contact (%.1f cm); hand %s target %s Mike %s"),FVector::Distance(Hand,Target),*Hand.ToString(),*Target.ToString(),*Mike->GetActorLocation().ToString()),FVector::Distance(Hand,Target)<35);
                TestTrue(TEXT("Earned uppercut launches even a nonfinal round"),D->bLaunched);
                TestTrue(TEXT("Final uppercut transfers to ragdoll"),Boss->GetMesh()->IsSimulatingPhysics());
                auto* LizardMesh=Boss->GetMesh();
                const FVector LeftHand=LizardMesh->GetSocketLocation(TEXT("hand_l"));
                const FVector RightHand=LizardMesh->GetSocketLocation(TEXT("hand_r"));
                const float Span=FVector::Distance(LeftHand,RightHand);
                TestTrue(FString::Printf(TEXT("Flyback starts with spread arms (%.1f cm span)"),Span),Span>100.f);
                for(const FName Side : {FName(TEXT("l")),FName(TEXT("r"))})
                {
                    const FString S=Side.ToString();
                    const FVector Shoulder=LizardMesh->GetSocketLocation(FName(*("upperarm_"+S)));
                    const FVector LizardHand=LizardMesh->GetSocketLocation(FName(*("hand_"+S)));
                    TestTrue(FString::Printf(TEXT("Flyback %s arm is near T-pose (%.1f cm height difference)"),*S,LizardHand.Z-Shoulder.Z),FMath::Abs(LizardHand.Z-Shoulder.Z)<25.f);
                    const FVector Hip=LizardMesh->GetSocketLocation(FName(*("thigh_"+S)));
                    const FVector Knee=LizardMesh->GetSocketLocation(FName(*("calf_"+S)));
                    const FVector Ankle=LizardMesh->GetSocketLocation(FName(*("foot_"+S)));
                    TestTrue(TEXT("Flyback knees bend instead of standing straight"),FVector::DotProduct((Knee-Hip).GetSafeNormal(),(Ankle-Knee).GetSafeNormal())<.95f);
                }
                TestTrue(TEXT("Flyback physics runs in slow motion"),FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World),.25f));
                TestTrue(TEXT("Flyback facing mirrors the published launch yaw"),FMath::IsNearlyEqual(Boss->GetActorRotation().Yaw,Facing>0?120.f:60.f,.1f));
                TestEqual(TEXT("Uppercut contact adds no combo point"),Mike->GetHitComboCount(),5);
            }
        };
        for(int32 Number=1;Number<=Prompts;++Number)
        {
            for(int32 I=0;I<1500&&D->IsActive()&&!D->QTE->IsPromptOpen();++I) Step();
            if(!TestTrue(TEXT("Prefix reaches each prompt"),D->IsActive()&&D->QTE->IsPromptOpen())) return false;
            for(int32 I=0;I<12;++I) Step();
            const FVector Held=Mike->GetActorLocation();
            if(Number>=3&&Number<=4) TestTrue(TEXT("Chomp and Claw prompts stay airborne"),Held.Z>D->MikeGroundZ+30);
            if(Number==5) TestTrue(TEXT("Fifth prompt opens at the grounded uppercut beginning"),FMath::IsNearlyEqual(Held.Z,D->MikeGroundZ,.1f)&&D->HeldPoseFraction<.12f);
            if(Number==6) TestTrue(TEXT("X opens only after authored landing"),FMath::IsNearlyEqual(Held.Z,D->MikeGroundZ,.1f));
            const FVector HeldLizard=Boss->GetMesh()->GetSocketLocation(TEXT("head"));
            const FVector HeldLeftHand=Boss->GetMesh()->GetSocketLocation(TEXT("hand_l"));
            const FVector HeldRightHand=Boss->GetMesh()->GetSocketLocation(TEXT("hand_r"));
            const float HeldFlight=D->FlightAge;
            for(int32 I=0;I<10;++I) Step();
            TestEqual(TEXT("Prompt placement remains frozen"),Mike->GetActorLocation(),Held);
            if(bFail&&Number==3)
            {
                D->Submit(EQuasicomboQTEInput::Jump);
                TestEqual(TEXT("Airborne failure cannot snap to the ground"),Mike->GetActorLocation(),Held);
                TestEqual(TEXT("Airborne failure clears combo"),Mike->GetHitComboCount(),0);
                Step();
                TestTrue(TEXT("Failure begins a continuous descent"),FVector::Distance(Mike->GetActorLocation(),Held)<15);
                break;
            }
            if(Number==6)
            {
                HUD->NativeTick(FGeometry(),.02f);
                auto* Instruction=Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("PromptText")));
                TestTrue(TEXT("X prompt explains Hold"),Instruction&&Instruction->GetText().ToString()==TEXT("Hold"));
                auto* Icon=Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("PromptIconText")));
                TestTrue(TEXT("Buttons have a stronger glyph stroke"),Icon&&Icon->GetFont().OutlineSettings.OutlineSize>=3);
                TestTrue(TEXT("X freezes the launched ragdoll"),D->bFlightFrozen&&Boss->GetMesh()->IsSimulatingPhysics());
                TestTrue(TEXT("Ragdoll pose remains still during X"),Boss->GetMesh()->GetSocketLocation(TEXT("head")).Equals(HeldLizard,.1f));
                TestTrue(TEXT("Both spread arms freeze with X"),Boss->GetMesh()->GetSocketLocation(TEXT("hand_l")).Equals(HeldLeftHand,.1f)&&Boss->GetMesh()->GetSocketLocation(TEXT("hand_r")).Equals(HeldRightHand,.1f));
                TestEqual(TEXT("X freezes the trajectory clock"),D->FlightAge,HeldFlight);
                TestTrue(TEXT("Charge camera retains a wide field of view"),D->Camera->GetCameraComponent()->FieldOfView>=50);
                D->QTE->Sequence.Submit(EQuasicomboQTEInput::Fire,FPlatformTime::Seconds()-1.1,false);
                D->Accepted(6,true);
                if(D->ChargeAudio) TestEqual(TEXT("Charge sound keeps normal volume during QTE"),D->ChargeAudio->VolumeMultiplier,1.f);
                HUD->NativeTick(FGeometry(),.02f);
                TestTrue(TEXT("Completed charge explains Release"),Instruction&&Instruction->GetText().ToString()==TEXT("Release"));
                for(int32 I=0;I<10;++I) Step();
                TestEqual(TEXT("Hold and Release keep the flight clock frozen"),D->FlightAge,HeldFlight);
                TestTrue(TEXT("Charge keeps the ragdoll pose frozen"),Boss->GetMesh()->GetSocketLocation(TEXT("head")).Equals(HeldLizard,.1f));
                D->Submit(EQuasicomboQTEInput::Fire,EQuasicomboQTEInputEdge::Release);
                TestFalse(TEXT("Release resumes ragdoll flight"),D->bFlightFrozen);
            }
            else if(Number==3)
            {
                HUD->NativeTick(FGeometry(),.02f);
                auto* Icon=Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("PromptIconText")));
                TestTrue(TEXT("Chomp shows Y"),Icon&&Icon->GetText().ToString()==TEXT("Y"));
                Mike->DoInteractStart();
            }
            else if(Number==4)
            {
                HUD->NativeTick(FGeometry(),.02f);
                auto* Icon=Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("PromptIconText")));
                TestTrue(TEXT("Claw shows RT"),Icon&&Icon->GetText().ToString()==TEXT("RT"));
                Mike->DoChargedAttackStart();
                Mike->DoChargedAttackEnd();
            }
            else D->Submit(D->QTE->GetExpectedInput());
            TestEqual(TEXT("Exactly one combo point per prompt"),Mike->GetHitComboCount(),Number);
            Step();
        }
        for(int32 I=0;I<1500&&D->IsActive();++I) Step();
        TestFalse(TEXT("Selected prefix finishes and releases ownership"),D->IsActive());
        // Walking adds a small floor clearance after collision is restored.
        TestTrue(FString::Printf(TEXT("Recovery leaves Mike grounded (%.2f vs %.2f cm)"),Mike->GetActorLocation().Z,D->MikeGroundZ),FMath::IsNearlyEqual(Mike->GetActorLocation().Z,D->MikeGroundZ,3.f));
        if(Prompts>=3) TestTrue(TEXT("Continuation includes apex"),SawApex);
        if(Prompts>=5&&!bFail) TestTrue(TEXT("Selected fifth uppercut reaches contact"),SawFinisherContact);
        if(Prompts<5) TestTrue(TEXT("Short prefix plays its death animation"),SawDeathPose);
        // Finish destroys the director. Inspect its cleanup in that frame, before
        // further world ticks may release or recycle the destroyed actor/component.
        TestFalse(FString::Printf(TEXT("Cleanup releases QTE input (selection %d, facing %.0f, stage %d)"),Selection,Facing,int(D->Stage)),D->QTE->IsQTEActive());
        auto* Run=World->GetSubsystem<UQuasicomboRunSubsystem>();
        TestEqual(TEXT("Only equipped final victory ends the level automatically"),Run->GetOutcome(),Final&&!bFail&&bEquipped?EQuasicomboRunOutcome::Victory:EQuasicomboRunOutcome::Playing);
        TestEqual(TEXT("Only an unequipped final victory spawns a cannon"),UGameplayStatics::GetActorOfClass(World,AQuasicomboVictoryPickup::StaticClass())!=nullptr,Final&&!bFail&&!bEquipped);
        if(Final&&Prompts<6&&!bFail)
        {
            const FVector CorpseStart=Boss->GetMesh()->GetSocketLocation(TEXT("pelvis"));
            const float HeadStart=Boss->GetMesh()->GetSocketLocation(TEXT("head")).Z;
            for(int32 I=0;I<300;++I) Fixture.TickTestWorld(.02f);
            TestTrue(TEXT("The corpse cannot launch upward after death"),Boss->GetMesh()->GetSocketLocation(TEXT("pelvis")).Z<CorpseStart.Z+100);
            TestTrue(TEXT("Corpse handoff starts with bounded velocity"),Boss->GetMesh()->GetPhysicsLinearVelocity().Size()<600);
            TestTrue(TEXT("Completed corpse keeps gravity beyond the old forced-freeze deadline"),Boss->GetMesh()->IsGravityEnabled());
            TestTrue(TEXT("Completed corpse remains a physical ragdoll"),Boss->GetMesh()->IsSimulatingPhysics());
            if(Prompts<5)
            {
                const float HeadDrop=HeadStart-Boss->GetMesh()->GetSocketLocation(TEXT("head")).Z;
                TestTrue(FString::Printf(TEXT("Death stumble visibly collapses into ragdoll (%.1f cm head drop)"),HeadDrop),HeadDrop>10.f);
            }
        }
        if(!Final||bFail)
        {
            TestFalse(TEXT("Counter restores animated lizard"),Boss->GetMesh()->IsSimulatingPhysics());
            TestTrue(TEXT("Counter restores combat facing"),Boss->GetActorRotation().Equals(D->BossStart.Rotator(),.1f));
        }
        TestTrue(TEXT("Cleanup restores player collision"),Mike->GetActorEnableCollision());
        if(!Final||bFail)
        {
            HUD->NativeTick(FGeometry(),.02f);
            auto* BossStatus=Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("BossText")));
            TestTrue(TEXT("Counter restores the evolution HUD's boss status"),BossStatus&&BossStatus->GetVisibility()==ESlateVisibility::HitTestInvisible);
        }
        Fixture.ForwardErrorMessages(this);
    }
    return true;
}
#endif
