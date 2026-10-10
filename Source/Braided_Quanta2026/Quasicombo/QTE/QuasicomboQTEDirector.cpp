#include "QuasicomboQTEDirector.h"
#include "QuasicomboQTEComponent.h"
#include "QuasicomboQTEPresentation.h"
#include "QuasicomboBoss.h"
#include "QuasicomboEncounterLightingComponent.h"
#include "QuasicomboMusicComponent.h"
#include "SideScrollingGameMode.h"
#include "QuasicomboRunSubsystem.h"
#include "QuasicomboVictoryPickup.h"
#include "SideScrollingCharacter.h"
#include "BusterCannonComponent.h"
#include "QuasicomboLoadoutSubsystem.h"
#include "BusterTravelAppearance.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "QuasicomboHUDActor.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
void Tint(UParticleSystemComponent* FX,ASideScrollingCharacter* Player)
{
    if(!FX||!Player||!Player->GetGameInstance()) return;
    if(const auto* State=Player->GetGameInstance()->GetSubsystem<UQuasicomboLoadoutSubsystem>())
    {
        FX->SetVectorParameter(TEXT("BusterCoreColor"),BusterTravel::ResolveTint(State->GetMikeLightColor(),true));
        FX->SetVectorParameter(TEXT("BusterSparkColor"),BusterTravel::ResolveTint(State->GetMikeSparkColor(),false));
    }
}
float Ease(float X) { X=FMath::Clamp(X,0.f,1.f); return X*X*(3-2*X); }
}
AQuasicomboQTEDirector::AQuasicomboQTEDirector()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bTickEvenWhenPaused=true;
    QTE=CreateDefaultSubobject<UQuasicomboQTEComponent>(TEXT("QTE"));
    Presentation=TSoftObjectPtr<UQuasicomboQTEPresentation>(FSoftObjectPath(TEXT("/Game/Quasicombo/QTE/DA_QTEPresentation.DA_QTEPresentation")));
}
bool AQuasicomboQTEDirector::Start(AQuasicomboBoss* InBoss,ASideScrollingCharacter* Player,bool bTau,int32 Round,int32 Rounds,int32 InCount)
{
    Data=Presentation.LoadSynchronous();
    if(!InBoss||!Player||!Data||Data->Beats.Num()<5||!Data->FindSegment(TEXT("FinisherUppercut"))||!Data->FindSegment(TEXT("FinisherLanding")))
    {
        UE_LOG(LogTemp,Error,TEXT("QTE presentation assets are missing; run Project/AuthorQTECinematics.py."));
        return false;
    }
    if(Player->GetGameInstance()) Player->GetGameInstance()->OnInputDeviceConnectionChange.AddDynamic(this,&AQuasicomboQTEDirector::DeviceConnectionChanged);
    Boss=InBoss; Mike=Player; OriginalQTE=InBoss->QTE; InBoss->QTE=QTE;
    Count=FMath::Clamp(InCount,2,Player->GetBusterCannon()->IsEquipped()?6:5);
    bFinal=Round>=Rounds; bActive=true; LastClock=FPlatformTime::Seconds();
    OriginalDilation=UGameplayStatics::GetGlobalTimeDilation(this);
    OriginalMovement=Player->GetCharacterMovement()->MovementMode;
    bMikeCollision=Player->GetActorEnableCollision(); bBossCollision=InBoss->GetActorEnableCollision();
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,UUserWidget::StaticClass(),true);
    for(auto* Widget:Widgets) if(!Widget->IsA<UQuasicomboHUDWidget>())
    { CombatHUDs.Add(Widget); CombatHUDVisibility.Add(Widget->GetVisibility()); Widget->SetVisibility(ESlateVisibility::Hidden); }
    Player->PrepareForFinisher(); Player->SetQTEComboHeld(true);
    Player->GetCharacterMovement()->DisableMovement(); InBoss->GetCharacterMovement()->DisableMovement();
    Player->SetActorEnableCollision(false); InBoss->SetActorEnableCollision(false);
    MikeStart=Player->GetActorTransform(); BossStart=InBoss->GetActorTransform();
    MikeGroundZ=InBoss->GetActorLocation().Z-InBoss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FHitResult Floor; FCollisionQueryParams GroundQuery; GroundQuery.AddIgnoredActor(Player); GroundQuery.AddIgnoredActor(InBoss);
    const FVector GroundOrigin(InBoss->GetActorLocation().X,Player->GetActorLocation().Y,InBoss->GetActorLocation().Z);
    if(GetWorld()->LineTraceSingleByChannel(Floor,GroundOrigin+FVector(0,0,200),GroundOrigin-FVector(0,0,600),ECC_WorldStatic,GroundQuery))
        MikeGroundZ=Floor.ImpactPoint.Z+Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Direction=Player->GetActorLocation().X<=InBoss->GetActorLocation().X?1.f:-1.f;
    Player->SetActorRotation(FRotator(0,Direction>0?0:180,0));
    InBoss->SetActorRotation(FRotator(0,Direction>0?180:0,0));
    Camera=GetWorld()->SpawnActor<ACameraActor>();
    if(auto* PC=Cast<APlayerController>(Player->GetController()))
    {
        Controller=PC; OriginalView=PC->GetViewTarget();
        UpdateCamera(0); PC->SetViewTargetWithBlend(Camera,0.18f);
    }
    if(auto* Run=GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>()) { Run->SetQTEDirector(this); Run->OnRunEnded.AddDynamic(this,&AQuasicomboQTEDirector::RunEnded); }
    QTE->OnPromptAccepted.AddDynamic(this,&AQuasicomboQTEDirector::Accepted);
    QTE->OnChargeReleased.AddDynamic(this,&AQuasicomboQTEDirector::Released);
    QTE->OnQTEFinished.AddDynamic(this,&AQuasicomboQTEDirector::Result);
    QTE->BeginRound(bTau,Round,Rounds,Count);
    // The establishing movement is also protected from input and timer expiry.
    QTE->HoldPresentation();
    const FVector ApproachEnd(BossStart.GetLocation().X-Direction*125,MikeStart.GetLocation().Y,MikeGroundZ);
    // Smoothstep peaks at 1.5 times its average speed. Scale the duration so
    // ApproachSpeed is the upper speed bound, including starts far from the boss.
    SetStage(EStage::Enter,FMath::Max(.6f,1.5f*FVector::Distance(MikeStart.GetLocation(),ApproachEnd)/FMath::Max(50.f,Data->ApproachSpeed)));
    SegmentName=TEXT("Approach");
    Play(Player,Data->MikeApproach,1.f,true);
    Play(InBoss,Data->Beats[0].Lizard,1.f,true);
    return true;
}
bool AQuasicomboQTEDirector::Submit(EQuasicomboQTEInput Input,EQuasicomboQTEInputEdge Edge)
{
    return bActive&&QTE->SubmitEdge(Input,Edge);
}
void AQuasicomboQTEDirector::SetStage(EStage Next,float Seconds)
{
    Stage=Next; Age=0; Duration=FMath::Max(.01f,Seconds); bContact=false; SegmentName=NAME_None;
}
void AQuasicomboQTEDirector::Play(ACharacter* Character,UAnimSequence* Clip,float Seconds,bool bLoop,bool bHoldEnd,float BlendIn)
{
    if(!Character||!Clip||!Character->GetMesh()->GetAnimInstance()) return;
    auto* Anim=Character->GetMesh()->GetAnimInstance();
    Anim->StopAllMontages(.06f);
    const float Rate=bLoop?1.f:Clip->GetPlayLength()/FMath::Max(.01f,Seconds);
    auto* Montage=UAnimMontage::CreateSlotAnimationAsDynamicMontage(Clip,TEXT("DefaultSlot"),BlendIn,.1f,1.f,bLoop?10000:1);
    if(!Montage) return;
    // The live instance copies this flag in Montage_Play. Setting it afterward
    // does not prevent that instance from blending back to locomotion.
    Montage->bEnableAutoBlendOut=!bHoldEnd;
    if(Anim->Montage_Play(Montage,Rate/FMath::Max(.01f,UGameplayStatics::GetGlobalTimeDilation(this)))<=0) return;
    if(Character==Mike.Get()) { MikeMontage=Montage; MikeRate=Rate; bMikePoseDriven=false; }
    else { BossMontage=Montage; BossRate=Rate; bBossPoseDriven=false; }
}
void AQuasicomboQTEDirector::SetMikePose(float Fraction)
{
    if(!Mike.IsValid()||!MikeMontage.IsValid()) return;
    if(auto* Anim=Mike->GetMesh()->GetAnimInstance())
    {
        bMikePoseDriven=true;
        const float Position=FMath::Clamp(Fraction*MikeMontage->GetPlayLength(),0.f,FMath::Max(0.f,MikeMontage->GetPlayLength()-.001f));
        Anim->Montage_SetPosition(MikeMontage.Get(),Position);
        Anim->Montage_Pause(MikeMontage.Get());
    }
}
void AQuasicomboQTEDirector::BeginAnticipation()
{
    PromptBeat=Beat+1;
    if(PromptBeat==1)
    {
        HeldPoseFraction=FMath::Clamp(Data->JumpReadyPromptFraction,.1f,.98f);
        SetStage(EStage::Anticipation,Data->JumpReadyDuration);
        Play(Mike.Get(),Data->MikeJumpReady,Duration,false,true);
        SegmentName=TEXT("JumpReady");
    }
    else if(PromptBeat<=5)
    {
        const auto& Def=Data->Beats[PromptBeat-1];
        HeldPoseFraction=FMath::Clamp(Def.PromptFraction,.01f,FMath::Max(.01f,Def.ContactFraction-.05f));
        SetStage(EStage::Anticipation,Def.Duration);
        Play(Mike.Get(),Def.Mike,Duration,false,true);
        SegmentName=BeatSegment(PromptBeat);
    }
    else
    {
        const auto* S=Data->FindSegment(TEXT("Cannon"));
        HeldPoseFraction=S&&S->PromptFraction>0?S->PromptFraction:.1f;
        SetStage(EStage::Anticipation,S?S->Duration:1.f);
        Play(Mike.Get(),Data->MikeCharge,Duration,false,true);
        SegmentName=TEXT("Cannon");
    }
    if(!bLaunched) if(const auto* S=CurrentSegment())
    { Play(Boss.Get(),S->Lizard,Duration,false,true); SetBossPose(0); }
    SetMikePose(0);
}
void AQuasicomboQTEDirector::SetBossPose(float Fraction)
{
    if(!Boss.IsValid()||!BossMontage.IsValid()) return;
    if(auto* Anim=Boss->GetMesh()->GetAnimInstance())
    {
        bBossPoseDriven=true;
        Anim->Montage_SetPosition(BossMontage.Get(),FMath::Clamp(Fraction*BossMontage->GetPlayLength(),0.f,FMath::Max(0.f,BossMontage->GetPlayLength()-.001f)));
        Anim->Montage_Pause(BossMontage.Get());
    }
}
void AQuasicomboQTEDirector::UpdateMikeActionPosition(int32 Move,float T)
{
    if(Data->FindSegment(BeatSegment(Move)))
    {
        ApplySegmentPosition(T);
        return;
    }
    const FVector Ground(BossStart.GetLocation().X-Direction*125,MikeStart.GetLocation().Y,MikeGroundZ);
    if(Move==1)
    {
        const float Takeoff=Ease((T-.15f)/.85f);
        Mike->SetActorLocation(Ground+FVector(Direction*40*Takeoff,0,70*Takeoff));
    }
    else if(Move==2)
    {
        const float ContactFraction=Data->Beats[1].ContactFraction;
        const float Rise=Ease(T/ContactFraction);
        Mike->SetActorLocation(Ground+FVector(Direction*40,0,70+30*Rise));
    }
    else if(Move==5)
    {
        // Start from the preceding move's stance; avoid snapping backward at the cut.
        Mike->SetActorLocation(Ground+FVector(Direction*40,0,140*FMath::Sin(T*PI)));
    }
}
FName AQuasicomboQTEDirector::BeatSegment(int32 Move) const
{
    static const FName Names[]={TEXT("Jump"),TEXT("Uppercut"),TEXT("Chomp"),TEXT("Claw"),TEXT("FinisherUppercut")};
    return Move>=1&&Move<=5?Names[Move-1]:NAME_None;
}
const FQuasicomboQTESegment* AQuasicomboQTEDirector::CurrentSegment() const
{
    return Data?Data->FindSegment(SegmentName):nullptr;
}
void AQuasicomboQTEDirector::PlaySegment(FName Name)
{
    SegmentName=Name;
    if(const auto* S=CurrentSegment()) Play(Mike.Get(),S->Mike,S->Duration,false,true);
    SetMikePose(0);
}
void AQuasicomboQTEDirector::ApplySegmentPosition(float Fraction,bool bRecovery)
{
    if(!Mike.IsValid()) return;
    const auto* S=CurrentSegment(); if(!S||S->Samples.IsEmpty()) return;
    const auto Sample=S->Evaluate(Fraction);
    const FVector Ground(BossStart.GetLocation().X-Direction*125,MikeStart.GetLocation().Y,MikeGroundZ);
    // Mike's lateral contact correction rotates with his pose. Lizard flight and
    // camera depth remain in the scene frame so the launch stays in the background.
    FVector Offset=Sample.MikeOffset; Offset.X*=Direction; Offset.Y*=Direction;
    FVector Position=Ground+Offset;
    if(bRecovery) Position+=RecoveryCorrection*(1-Ease(Fraction));
    Mike->SetActorLocation(Position);
    if(!bLaunched&&!bRecovery&&Boss.IsValid())
    {
        const auto* Opening=Data->FindSegment(TEXT("Jump"));
        FVector Lizard=Sample.LizardOffset-(Opening?Opening->Evaluate(0).LizardOffset:FVector(125,0,32)); Lizard.X*=Direction;
        Boss->SetActorLocation(BossStart.GetLocation()+Lizard);
    }
}
void AQuasicomboQTEDirector::BeginTransition()
{
    const FName Name=Beat==5?FName(TEXT("CannonLanding")):Beat==4?FName(TEXT("FinisherLanding")):FName(TEXT("JumpApex"));
    const auto* S=Data->FindSegment(Name);
    SetStage(Beat==5?EStage::CannonLanding:Beat==4?EStage::FinisherLanding:EStage::Apex,S?S->Duration:.2f);
    PlaySegment(Name);
    if(!bLaunched&&S) { Play(Boss.Get(),S->Lizard,S->Duration,false,true); SetBossPose(0); }
}
void AQuasicomboQTEDirector::BeginRecovery()
{
    const auto* S=Data->FindSegment(TEXT("Recovery"));
    const FVector Ground(BossStart.GetLocation().X-Direction*125,MikeStart.GetLocation().Y,MikeGroundZ);
    FVector Offset=S?S->Evaluate(0).MikeOffset:FVector(40,0,100); Offset.X*=Direction; Offset.Y*=Direction;
    RecoveryCorrection=Mike.IsValid()?Mike->GetActorLocation()-(Ground+Offset):FVector::ZeroVector;
    PlaySegment(TEXT("Recovery"));
}
void AQuasicomboQTEDirector::Accepted(int32 Number,bool bChargeStart)
{
    if(!bActive||!Mike.IsValid()||!Boss.IsValid()) return;
    Mike->RegisterHitCombo(); Beat=Number;
    if(bChargeStart)
    {
        SetStage(EStage::Charge,float(FQuasicomboQTESequence::ChargeHoldSeconds+FQuasicomboQTESequence::ReleaseWindowSeconds));
        // Continue the held aim pose. The charge clock belongs to QTE, not the clip.
        SegmentName=TEXT("Cannon");
        auto* Cannon=Mike->GetBusterCannon();
        if(Data->ChargeEffect) ChargeFX=UGameplayStatics::SpawnEmitterAttached(Data->ChargeEffect,Cannon->CannonMesh,
            NAME_None,Cannon->MuzzleLocalTransform.GetLocation(),FRotator::ZeroRotator,FVector(.15f),
            EAttachLocation::KeepRelativeOffset,false,EPSCPoolMethod::None,false);
        Tint(ChargeFX,Mike.Get()); if(ChargeFX) ChargeFX->Activate();
        if(Data->ChargeSound) ChargeAudio=UGameplayStatics::SpawnSoundAttached(Data->ChargeSound,Cannon->CannonMesh);
        return;
    }
    const auto& Def=Data->Beats[Beat-1];
    const bool bContinueHeldClip=Stage==EStage::Prompt&&PromptBeat==Number&&Number>1;
    SetStage(EStage::Action,Def.Duration);
    SegmentName=BeatSegment(Beat);
    if(bContinueHeldClip) Age=HeldPoseFraction*Duration;
    else
    { Play(Mike.Get(),Def.Mike,Duration,false,true); Play(Boss.Get(),Def.Lizard,Duration,false,true); }
    SetMikePose(Age/Duration);
    if(!bLaunched) SetBossPose(Age/Duration);
    // Reactions start at contact, not as Mike begins his anticipation.
}
void AQuasicomboQTEDirector::Contact()
{
    // Takeoff is movement only, even if an older presentation asset marks it as a hit.
    if(bContact||Beat<=1||!Boss.IsValid()||!Data->Beats[Beat-1].bProducesContact) return;

    bContact=true;
    const auto& Def=Data->Beats[Beat-1];
    if (Mike.IsValid()) Mike->RecordCombatActivity();

    // The paired source keeps the lizard still until Contact, then reacts on
    // this same clock. Fifth contact transfers ownership to the launch segment.

    if(Def.ContactSound) UGameplayStatics::PlaySoundAtLocation(this,Def.ContactSound,Boss->GetActorLocation());

    const FVector MikeContact=Mike->GetMesh()->GetSocketLocation(Def.MikeContactBone);
    const FVector LizardContact=Boss->GetMesh()->GetSocketLocation(Def.LizardContactBone);
    if(Beat>=2) SpawnBurst((MikeContact+LizardContact)*.5f,false);
    HitStopUntil=FPlatformTime::Seconds()+.055;
    if(Beat==5)
    {
        LaunchStart=Boss->GetActorLocation(); bLaunched=true; bFlightFrozen=false; FlightAge=0;
        if(bFinal) SlowImpactRemaining=.6f;
        // The presentation clock/input deadlines remain real-time. Physics gets
        // a slow flyback until the prompt freezes it or the counter restores time.
        UGameplayStatics::SetGlobalTimeDilation(this,.25f);
        const auto* Launch=Data->FindSegment(TEXT("Launch"));
        if(Data->LizardFlight)
        {
            const float Yaw=Launch?Launch->Evaluate(0).LizardYaw:120.f;
            Boss->SetActorRotation(FRotator(0,Direction>0?Yaw:180.f-Yaw,0));
            // Evaluate the authored recoil before physics takes ownership. A
            // zero blend-in prevents seeding the ragdoll from the standing pose.
            Play(Boss.Get(),Data->LizardFlight,Launch?Launch->Duration:1.8f,false,true,0.f);
            SetBossPose(0);
            Boss->GetMesh()->TickAnimation(0.f,false);
            Boss->GetMesh()->RefreshBoneTransforms();
        }
        Boss->BeginCinematicRagdoll(true);
        LaunchPelvisStart=Boss->GetMesh()->GetSocketLocation(TEXT("pelvis"));
        Boss->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector(0,Direction*12,Direction*3),false,TEXT("pelvis"));
    }
}
void AQuasicomboQTEDirector::Released()
{
    if(!bActive||Stage!=EStage::Charge||!Boss.IsValid()||!Mike.IsValid()) return;
    StopCharge();
    bFlightFrozen=false;
    Boss->GetMesh()->WakeAllRigidBodies();
    Boss->GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector(0,Direction*35,0));
    const auto* Fire=Data->FindSegment(TEXT("CannonFire"));
    SetStage(EStage::Shot,Fire?Fire->Duration:.6f);
    ShotStart=Mike->GetBusterCannon()->GetMuzzleWorldTransform().GetLocation();
    ShotTarget=Boss->GetMesh()->GetSocketLocation(TEXT("spine_02"));
    Play(Mike.Get(),Data->MikeFire,Duration,false,true);
    SegmentName=TEXT("CannonFire");
    SetMikePose(0);
    if(Data->FireSound) UGameplayStatics::PlaySoundAtLocation(this,Data->FireSound,ShotStart);
    auto* Travel=LoadObject<UParticleSystem>(nullptr,TEXT("/Game/Quasicombo/Buster/VFX/P_BusterThunderTravel.P_BusterThunderTravel"));
    const auto* Cannon=Mike->GetBusterCannon();
    const float Scale=Cannon->TravelDiameterCm*Data->CinematicDiameterMultiplier/FMath::Max(1.f,Cannon->ThunderVisualDiameterCm);
    ShotFX=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),Travel,
        FTransform((ShotTarget-ShotStart).Rotation(),ShotStart,FVector(Scale)),false,EPSCPoolMethod::None,false);
    Tint(ShotFX,Mike.Get()); if(ShotFX) ShotFX->Activate();
    UGameplayStatics::SetGlobalTimeDilation(this,.25f);
}
void AQuasicomboQTEDirector::Result(bool bSucceeded)
{
    if(!bActive) return;
    bSuccess=bSucceeded; StopCharge();
    if(!bSucceeded&&Mike.IsValid()) Mike->ResetHitCombo();
    if(!bSucceeded)
    {
        // Keep the failed prompt's evaluated pose, placement and camera visible
        // while the HUD flashes the expected button, before recovery/counter.
        Stage=EStage::FailureFeedback;
        FailureFeedbackRemaining=FMath::Clamp(FailureFeedbackSeconds,.05f,2.f);
        for(int32 I=0;I<2;++I)
        {
            ACharacter* Character=I==0?static_cast<ACharacter*>(Mike.Get()):static_cast<ACharacter*>(Boss.Get());
            UAnimMontage* Montage=I==0?MikeMontage.Get():BossMontage.Get();
            if(Character&&Montage&&Character->GetMesh()->GetAnimInstance())
                Character->GetMesh()->GetAnimInstance()->Montage_Pause(Montage);
        }
        if(bLaunched) FreezeFlight();
        return;
    }
    if(!bFinal) { BeginCounter(); return; }
    if(Count==6)
    {
        ImpactPosition=ShotTarget; SetStage(EStage::Blast,1.35f);
        if (Mike.IsValid()) Mike->RecordCombatActivity();
        SpawnBurst(ImpactPosition,true);
        BeginRecovery();
    }
    else
    {
        SetStage(EStage::Fall,Count==5?1.6f:(Boss.IsValid()&&Boss->DefeatAnimation?Boss->DefeatAnimation->GetPlayLength():1.5f));
        if(Count<5&&Boss.IsValid()) { Play(Boss.Get(),Boss->DefeatAnimation,Duration,false,true); SetBossPose(0); }
        BeginRecovery();
    }
}
void AQuasicomboQTEDirector::BeginCounter()
{
    SlowImpactRemaining=0; UGameplayStatics::SetGlobalTimeDilation(this,OriginalDilation);
    CounterStart=Boss.IsValid()?Boss->GetActorLocation():BossStart.GetLocation();
    MikeCounterStart=Mike.IsValid()?Mike->GetActorLocation():MikeStart.GetLocation();
    if(Boss.IsValid()) { Boss->RestoreCinematicRagdoll(); Boss->SetActorRotation(BossStart.GetRotation()); }
    bFlightFrozen=false;
    const auto* Recovery=Data->FindSegment(TEXT("Recovery"));
    const auto* Counter=Data->FindSegment(TEXT("Counter"));
    bCounterStarted=false; SetStage(EStage::Counter,(Recovery?Recovery->Duration:.6f)+.25f+(Counter?Counter->Duration:.65f));
    bLaunched=false;
    Play(Boss.Get(),Data->LizardRecover,.4f);
    BeginRecovery();
}
void AQuasicomboQTEDirector::Rumble(float Strength)
{
    auto* PC=Controller.Get();
    if(!PC) { RumbleHandle=0; return; }
    if(Strength>0&&PC->bForceFeedbackEnabled)
        RumbleHandle=PC->PlayDynamicForceFeedback(Strength,-1,true,true,true,true,
            RumbleHandle?EDynamicForceFeedbackAction::Update:EDynamicForceFeedbackAction::Start,RumbleHandle);
    else if(RumbleHandle)
    {
        PC->PlayDynamicForceFeedback(0,0,true,true,true,true,EDynamicForceFeedbackAction::Stop,RumbleHandle);
        RumbleHandle=0;
    }
}
void AQuasicomboQTEDirector::StopCharge()
{
    Rumble(0);
    if(IsValid(ChargeFX)) { ChargeFX->Deactivate(); ChargeFX->DestroyComponent(); }
    ChargeFX=nullptr;
    if(IsValid(ChargeAudio)) { ChargeAudio->Stop(); ChargeAudio->DestroyComponent(); }
    ChargeAudio=nullptr;
}
void AQuasicomboQTEDirector::SpawnBurst(FVector Position,bool bHuge)
{
    if(!Data||!Mike.IsValid()) return;
    if(Data->ImpactEffect)
    {
        auto* FX=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),Data->ImpactEffect,
            FTransform(FRotator::ZeroRotator,Position,FVector(bHuge?Data->ExplosionScale:.18f)),
            true,EPSCPoolMethod::None,false);
        Tint(FX,Mike.Get()); if(FX) { FX->CustomTimeDilation=1.f/FMath::Max(.01f,UGameplayStatics::GetGlobalTimeDilation(this)); ImpactFX.Add(FX); FX->Activate(); }
    }
    if(bHuge&&Data->ElectricityEffect)
    {
        auto* FX=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),Data->ElectricityEffect,
            FTransform(FRotator::ZeroRotator,Position,FVector(Data->ExplosionScale)),
            true,EPSCPoolMethod::None,false);
        Tint(FX,Mike.Get()); if(FX) { FX->CustomTimeDilation=1.f/FMath::Max(.01f,UGameplayStatics::GetGlobalTimeDilation(this)); ImpactFX.Add(FX); FX->Activate(); }
    }
    if(bHuge&&Data->ImpactSound) UGameplayStatics::PlaySoundAtLocation(this,Data->ImpactSound,Position);
}
void AQuasicomboQTEDirector::UpdateFlight(float Delta)
{
    if(!Boss.IsValid()||!bLaunched||bFlightFrozen) return;
    FlightAge+=Delta;
    const auto* Launch=Data->FindSegment(TEXT("Launch"));
    const float LaunchDuration=Launch?Launch->Duration:1.8f;
    const float T=FMath::Clamp(FlightAge/LaunchDuration,0.f,1.f);
    FVector Local=Launch?Launch->Evaluate(T).LizardOffset-Launch->Evaluate(0).LizardOffset:FVector(650*Ease(T),-220*Ease(T),FMath::Sin(T*PI)*280);
    Local.X*=Direction;
    FVector End=LaunchStart+Local;
    FHitResult Hit; FCollisionQueryParams Query; Query.AddIgnoredActor(Boss.Get()); Query.AddIgnoredActor(Mike.Get());
    if(GetWorld()->LineTraceSingleByChannel(Hit,BossStart.GetLocation(),End,ECC_WorldStatic,Query))
        End.X=Hit.Location.X-Direction*70.f;
    auto* Mesh=Boss->GetMesh();
    if(Mesh->IsSimulatingPhysics())
    {
        // Physics supplies the loose pose; the authored path owns its translation.
        const FVector PelvisTarget=LaunchPelvisStart+(End-LaunchStart);
        Mesh->AddWorldOffset(PelvisTarget-Mesh->GetSocketLocation(TEXT("pelvis")),false,nullptr,ETeleportType::TeleportPhysics);
        Mesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
    }
    Boss->SetActorLocation(End,false,nullptr,ETeleportType::TeleportPhysics);
}
void AQuasicomboQTEDirector::FreezeFlight()
{
    if(!Boss.IsValid()||!bLaunched) return;
    bFlightFrozen=true;
    auto* Mesh=Boss->GetMesh();
    Mesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
    Mesh->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    Mesh->PutAllRigidBodiesToSleep();
}
void AQuasicomboQTEDirector::UpdateCamera(float Alpha)
{
    if(!Camera||!Mike.IsValid()) return;
    const FVector M=Mike->GetActorLocation()+FVector(0,0,40);
    const FVector B=Boss.IsValid()?Boss->GetActorLocation()+FVector(0,0,40):ImpactPosition;
    FVector Target=(M+B)*.5f,Offset(-Direction*110,520,140); float FOV=48;
    const int32 CameraBeat=(Stage==EStage::Anticipation||Stage==EStage::Prompt)?PromptBeat:Beat;
    if((Stage==EStage::Action||Stage==EStage::Anticipation||Stage==EStage::Prompt)&&Data&&Data->Beats.IsValidIndex(CameraBeat-1))
    { Offset=Data->Beats[CameraBeat-1].CameraOffset; Offset.X*=Direction; FOV=Data->Beats[CameraBeat-1].FOV; }
    if(Stage==EStage::Charge)
    {
        Target=(M+B)*.5f;
        Offset=FVector(-Direction*100,850,180); FOV=52;
    }
    if(Stage==EStage::Shot) { Target=FMath::Lerp(M,B,Alpha); Offset=FVector(-Direction*100,650,150); FOV=52; }
    if(Stage==EStage::Blast) { Target=ImpactPosition; Offset=FVector(-Direction*160,850,180); FOV=57; }
    if(Stage==EStage::Fall) { Target=B; Offset=FVector(-Direction*80,700,120); FOV=52; }
    if(const auto* S=CurrentSegment()) if(!S->Samples.IsEmpty())
    {
        float SampleTime=(Stage==EStage::Counter||Stage==EStage::Fall||Stage==EStage::Blast)?Age/FMath::Max(.01f,S->Duration):Alpha;
        if(Stage==EStage::Counter&&SegmentName==TEXT("Counter"))
        { const auto* R=Data->FindSegment(TEXT("Recovery")); SampleTime=(Age-(R?R->Duration:.6f)-.25f)/FMath::Max(.01f,S->Duration); }
        const auto Sample=S->Evaluate(SampleTime);
        // Focus follows live subjects, including the lizard during landing and charge.
        Target=FMath::Lerp(Mike->GetActorLocation(),Boss.IsValid()?Boss->GetActorLocation():ImpactPosition,Sample.TargetBlend);
        FVector Focus=Sample.FocusOffset; Focus.X*=Direction; Target+=Focus;
        Offset=Sample.CameraOffset; Offset.X*=Direction; FOV=Sample.FOV;
    }
    const FVector Eye=Target+Offset;
    FHitResult Hit; FCollisionQueryParams Query; Query.AddIgnoredActor(Mike.Get()); Query.AddIgnoredActor(Boss.Get());
    FVector SafeEye=Eye;
    if(GetWorld()->LineTraceSingleByChannel(Hit,Target,Eye,ECC_WorldStatic,Query)) SafeEye=Hit.Location+(Target-Hit.Location).GetSafeNormal()*25;
    Camera->SetActorLocation(SafeEye); Camera->SetActorRotation((Target-SafeEye).Rotation());
    Camera->GetCameraComponent()->SetFieldOfView(FOV);
}
void AQuasicomboQTEDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const double Now=FPlatformTime::Seconds(); const float Delta=FMath::Clamp(float(Now-LastClock),0.f,.1f); LastClock=Now;
    if(!bActive) return;
    const auto* Run=GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
    if(!Mike.IsValid()||(!Boss.IsValid()&&!bObliterated)||!Run||Run->GetOutcome()!=EQuasicomboRunOutcome::Playing) { Cleanup(); Destroy(); return; }
    if(GetWorld()->IsPaused()) { Rumble(0); return; }
    if(Stage==EStage::FailureFeedback)
    {
        if(bLaunched) FreezeFlight();
        FailureFeedbackRemaining-=Delta;
        if(FailureFeedbackRemaining<=0) BeginCounter();
        return;
    }
    const bool bHitStop=Now<HitStopUntil;
    const float ImpactRate=SlowImpactRemaining>0?.25f:1.f;
    for(int32 I=0;I<2;++I)
    {
        ACharacter* C=I==0?static_cast<ACharacter*>(Mike.Get()):static_cast<ACharacter*>(Boss.Get());
        UAnimMontage* Montage=I==0?MikeMontage.Get():BossMontage.Get();
        if(C&&Montage&&C->GetMesh()->GetAnimInstance())
        {
            auto* Anim=C->GetMesh()->GetAnimInstance();
        if((I==0&&bMikePoseDriven)||(I==1&&bBossPoseDriven)) Anim->Montage_Pause(Montage);
            else if(bHitStop) Anim->Montage_Pause(Montage);
            else { Anim->Montage_Resume(Montage); Anim->Montage_SetPlayRate(Montage,ImpactRate*(I==0?MikeRate:BossRate)/FMath::Max(.01f,UGameplayStatics::GetGlobalTimeDilation(this))); }
        }
    }
    if(bHitStop) return;
    SlowImpactRemaining=FMath::Max(0.f,SlowImpactRemaining-Delta);
    if(Stage!=EStage::Prompt) Age+=Delta*(Stage==EStage::Action?ImpactRate:1.f);
    const float T=FMath::Clamp(Age/Duration,0.f,1.f);
    if(Stage==EStage::Enter)
    {
        FVector End=BossStart.GetLocation()+FVector(-Direction*125,MikeStart.GetLocation().Y-BossStart.GetLocation().Y,MikeGroundZ-BossStart.GetLocation().Z);
        Mike->SetActorLocation(FMath::Lerp(MikeStart.GetLocation(),End,Ease(T)));
        if(const auto* S=CurrentSegment())
        {
            FVector Offset=S->Evaluate(T).MikeOffset; Offset.X*=Direction; Offset.Y*=Direction;
            FVector First=S->Evaluate(0).MikeOffset; First.X*=Direction; First.Y*=Direction;
            Mike->SetActorLocation(End+Offset+(MikeStart.GetLocation()-End-First)*(1-Ease(T)));
        }
        if(T>=1) BeginAnticipation();
    }
    else if(Stage==EStage::Apex||Stage==EStage::FinisherLanding||Stage==EStage::CannonLanding)
    {
        SetMikePose(T); ApplySegmentPosition(T);
        if(!bLaunched) SetBossPose(T);
        if(bLaunched) UpdateFlight(Delta);
        if(T>=1) BeginAnticipation();
    }
    else if(Stage==EStage::Anticipation)
    {
        const float PoseFraction=FMath::Min(T,HeldPoseFraction);
        SetMikePose(PoseFraction);
        if(!bLaunched) SetBossPose(PoseFraction);
        if(PromptBeat>=2&&PromptBeat<=5) UpdateMikeActionPosition(PromptBeat,PoseFraction);
        else ApplySegmentPosition(PoseFraction);
        if(PromptBeat==6) UpdateFlight(Delta);
        if(T>=HeldPoseFraction)
        {
            Stage=EStage::Prompt; Age=HeldPoseFraction*Duration;
            if(PromptBeat==6) FreezeFlight();
            // Publish the glyph only after that live animation is paused.
            if(Beat==0) QTE->OpenPresentation();
            else QTE->CompleteAction();
        }
    }
    else if(Stage==EStage::Action)
    {
        const auto& Def=Data->Beats[Beat-1];
        SetMikePose(T);
        if(!bLaunched) SetBossPose(T);
        UpdateMikeActionPosition(Beat,T);
        if(T>=Def.ContactFraction) Contact();
        if(bContact&&Beat==5) UpdateFlight(Delta*ImpactRate);
        if(T>=1)
        {
            if(QTE->IsFinishingBeat()) QTE->CompleteAction();
            else if(Beat>=2) BeginTransition();
            else BeginAnticipation();
        }
    }
    else if(Stage==EStage::Prompt) { if(bFlightFrozen) FreezeFlight(); }
    else if(Stage==EStage::Charge)
    {
        const float Charge=FMath::Clamp(QTE->GetChargeProgress(),0.f,1.f);
        SetMikePose(FMath::Lerp(HeldPoseFraction,1.f,Charge));
        ApplySegmentPosition(FMath::Lerp(HeldPoseFraction,1.f,Charge));
        Rumble(FMath::Lerp(.15f,.6f,Charge));
        if(ChargeFX) ChargeFX->SetRelativeScale3D(FVector(.15f+.65f*Ease(Charge)));
        if(bFlightFrozen) FreezeFlight();
    }
    else if(Stage==EStage::Shot)
    {
        SetMikePose(T); ApplySegmentPosition(T);
        UpdateFlight(Delta);
        if(Boss.IsValid()) ShotTarget=Boss->GetMesh()->GetSocketLocation(TEXT("spine_02"));
        if(ShotFX) ShotFX->SetWorldLocation(FMath::Lerp(ShotStart,ShotTarget,T));
        if(T>=1) { if(ShotFX) { ShotFX->DestroyComponent(); ShotFX=nullptr; } QTE->CompleteAction(); }
    }
    else if(Stage==EStage::Counter)
    {
        if(Boss.IsValid()) Boss->SetActorLocation(FMath::Lerp(CounterStart,BossStart.GetLocation(),Ease(Age/.4f)));
        const auto* Recovery=Data->FindSegment(TEXT("Recovery"));
        const float LandTime=Recovery?Recovery->Duration:.6f;
        const auto* Counter=Data->FindSegment(TEXT("Counter"));
        const float KnockTime=Counter?Counter->Duration:.65f;
        if(Age<=LandTime) { SetMikePose(Age/LandTime); ApplySegmentPosition(Age/LandTime,true); }
        if(Age>=LandTime&&!bCounterStarted)
        { bCounterStarted=true; Play(Boss.Get(),Data->LizardCounter,.9f); }
        if(Age>=LandTime+.25f&&!bContact)
        {

            bContact=true; Mike->ApplyQTECounterDamage();

            Play(Mike.Get(),Data->MikeCounterReaction,KnockTime,false,true);
            SegmentName=TEXT("Counter");
            const auto* S=CurrentSegment();
            const FVector Ground(BossStart.GetLocation().X-Direction*125,MikeStart.GetLocation().Y,MikeGroundZ);
            FVector Offset=S?S->Evaluate(0).MikeOffset:FVector(40,0,0); Offset.X*=Direction; Offset.Y*=Direction;
            RecoveryCorrection=Mike->GetActorLocation()-(Ground+Offset);
        }
        if(bContact)
        {
            const float Knock=FMath::Clamp((Age-LandTime-.25f)/KnockTime,0.f,1.f);
            const FVector Start=Mike->GetActorLocation();
            SetMikePose(Knock); ApplySegmentPosition(Knock,true);
            FVector End=Mike->GetActorLocation();
            FHitResult Hit; FCollisionQueryParams Query; Query.AddIgnoredActor(Mike.Get()); Query.AddIgnoredActor(Boss.Get());
            if(GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_WorldStatic,Query)) End=Hit.Location+FVector(Direction*65,0,0);
            Mike->SetActorLocation(End);
        }
        if(T>=1) { Finish(); }
    }
    else if(Stage==EStage::Fall)
    {
        if(const auto* S=CurrentSegment()) { const float R=Age/FMath::Max(.01f,S->Duration); SetMikePose(R); ApplySegmentPosition(R,true); }
        if(Count==5) UpdateFlight(Delta);
        else SetBossPose(T);
        if(T>=1) { Finish(); }
    }
    else if(Stage==EStage::Blast)
    {
        if(const auto* S=CurrentSegment()) { const float R=Age/FMath::Max(.01f,S->Duration); SetMikePose(R); ApplySegmentPosition(R,true); }
        // Body removal is deliberately hidden under the dense peak of the explosion.
        if(Age>=.18f&&!bObliterated&&Boss.IsValid())
        {
            // World lifetime keeps the dim reward scene after the body is removed.
            if(auto* RewardOwner=GetWorld()->SpawnActor<AActor>())
                Boss->EncounterLighting->TransferToRewardOwner(RewardOwner);
            bObliterated=true; Boss->Destroy();
        }
        if(T>=1) { Finish(); }
    }
    const float CameraTime=Stage==EStage::Charge?FMath::Lerp(HeldPoseFraction,1.f,FMath::Clamp(QTE->GetChargeProgress(),0.f,1.f)):T;
    UpdateCamera(CameraTime);
}
void AQuasicomboQTEDirector::Finish()
{
    if(!bActive) return;

    bResolved=true;
    bool bFinishWithoutPickup=false;
    if(Boss.IsValid()) Boss->QTE=OriginalQTE.Get();
    if(bSuccess&&bFinal)
    {
        if(auto* Mode=Cast<ASideScrollingGameMode>(UGameplayStatics::GetGameMode(this)))
            Mode->MusicController->StopMusic(.8f);
        if(auto* Run=GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>()) Run->MarkBossDefeated();
        if(Boss.IsValid()) Boss->FinishCinematicDefeat();
        bFinishWithoutPickup=Mike->GetBusterCannon()->IsEquipped();
        if(!bFinishWithoutPickup)
        {
            FVector Position=Mike->GetActorLocation()+FVector(Direction*130,0,0);
            FHitResult Ground; FCollisionQueryParams Query; Query.AddIgnoredActor(Mike.Get()); Query.AddIgnoredActor(Boss.Get());
            if(GetWorld()->LineTraceSingleByChannel(Ground,Position+FVector(0,0,200),Position-FVector(0,0,600),ECC_WorldStatic,Query))
                Position.Z=Ground.ImpactPoint.Z+75;
            auto* Reward=GetWorld()->SpawnActor<AQuasicomboVictoryPickup>(Position,FRotator::ZeroRotator);
            if(Reward) Reward->Initialize(nullptr,Mike.Get());
            else UE_LOG(LogTemp,Error,TEXT("QTE victory pickup failed to spawn"));
        }
    }
    else if(Boss.IsValid()) { Boss->ResetAfterQTERound(bSuccess); }
    Cleanup();
    // Release animation, camera and input before the victory screen can open.
    if(bFinishWithoutPickup)
        if(auto* Run=GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>()) Run->EndRun(EQuasicomboRunOutcome::Victory);
    Destroy();
}
void AQuasicomboQTEDirector::Cleanup()
{
    if(!bActive) return;

    bActive=false; StopCharge();
    if(GetGameInstance()) GetGameInstance()->OnInputDeviceConnectionChange.RemoveDynamic(this,&AQuasicomboQTEDirector::DeviceConnectionChanged); QTE->CancelQTE();
    if(ShotFX) { ShotFX->DestroyComponent(); ShotFX=nullptr; }
    for(UParticleSystemComponent* FX:ImpactFX) if(IsValid(FX)) FX->DestroyComponent();
    ImpactFX.Reset();
    for(int32 I=0;I<CombatHUDs.Num();++I) if(CombatHUDs[I].IsValid()) CombatHUDs[I]->SetVisibility(CombatHUDVisibility[I]);
    if(!bResolved)
    {
        if(Mike.IsValid()) Mike->SetActorTransform(MikeStart);
        if(Boss.IsValid())
        {
            Boss->RestoreCinematicRagdoll();
            Boss->SetActorTransform(BossStart);
            const auto* Run=GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
            if(Run&&Run->GetOutcome()==EQuasicomboRunOutcome::Playing&&Boss->GetBossPhase()==EQuasicomboBossPhase::QTE) Boss->ResetAfterQTERound(false);
        }
    }
    UGameplayStatics::SetGlobalTimeDilation(this,OriginalDilation);
    if(Mike.IsValid())
    {
        Mike->GetMesh()->bPauseAnims=false;
        if(auto* Anim=Mike->GetMesh()->GetAnimInstance()) Anim->StopAllMontages(.12f);

        Mike->SetActorEnableCollision(bMikeCollision);

        Mike->GetCharacterMovement()->SetMovementMode(OriginalMovement==MOVE_None?MOVE_Walking:OriginalMovement);

        Mike->SetQTEComboHeld(false);

    }
    if(auto* PC=Controller.Get())
    {
        AActor* Target=OriginalView.IsValid()?OriginalView.Get():PC->GetPawn();
        PC->SetViewTargetWithBlend(Target?Target:PC,.25f);
    }
    if(Boss.IsValid())
    {
        Boss->QTE=OriginalQTE.Get();
        if(auto* Anim=Boss->GetMesh()->GetAnimInstance())
            if(BossMontage.IsValid()) Anim->Montage_Stop(.12f,BossMontage.Get());

        if(!bSuccess||!bFinal) Boss->SetActorEnableCollision(bBossCollision);
    }
    if(auto* Run=GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>()) { Run->SetQTEDirector(nullptr); Run->OnRunEnded.RemoveDynamic(this,&AQuasicomboQTEDirector::RunEnded); }
    if(Camera) { Camera->SetLifeSpan(.4f); Camera=nullptr; }
}
void AQuasicomboQTEDirector::EndPlay(const EEndPlayReason::Type Reason) { Cleanup(); Super::EndPlay(Reason); }
void AQuasicomboQTEDirector::DeviceConnectionChanged(EInputDeviceConnectionState State,FPlatformUserId User,FInputDeviceId Device)
{
    if(State==EInputDeviceConnectionState::Disconnected)
    {
        Rumble(0);
        if(bActive&&QTE->IsCharging()) Submit(EQuasicomboQTEInput::Fire,EQuasicomboQTEInputEdge::Cancel);
    }
}
#if WITH_DEV_AUTOMATION_TESTS
void AQuasicomboQTEDirector::AdvanceForTesting(float Seconds)
{
    while(bActive&&Seconds>0)
    {
        const float Step=FMath::Min(.05f,Seconds);
        HitStopUntil=0; LastClock=FPlatformTime::Seconds()-Step; Tick(Step); Seconds-=Step;
    }
}
void AQuasicomboQTEDirector::AdvanceToPromptForTesting()
{
    for(int32 I=0;I<2000&&bActive&&!QTE->IsPromptOpen();++I) AdvanceForTesting(.05f);
}
#endif

void AQuasicomboQTEDirector::RunEnded(EQuasicomboRunOutcome) { Cleanup(); Destroy(); }
