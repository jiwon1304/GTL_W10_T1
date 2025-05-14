#include "AnimInstance.h"

#include "AnimationStateMachine.h"
#include "AnimSequenceBase.h"
#include "AnimData/AnimDataModel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Engine/Asset/SkeletalMeshAsset.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Math/Transform.h"
#include "UObject/Casts.h"
#include "AnimTypes.h"
#include "AnimNotifies/AnimNotifyState.h"
#include "Engine/FbxManager.h"
#include "LuaScripts/LuaScriptFileUtils.h"
#include "World/World.h"
#include "sol/sol.hpp"

UAnimInstance::UAnimInstance()
    : Sequence(nullptr)
    , OwningComp(nullptr)
    , CurrentTime(0.0f)
    , bPlaying(false)
{
    
}

void UAnimInstance::InitializeAnimation(USkeletalMeshComponent* InOwningComponent)
{
    OwningComp = InOwningComponent;

    if (ScriptPath.IsEmpty()) {
        bool bSuccess = LuaScriptFileUtils::MakeScriptPathAndDisplayName(
            L"template_asm.lua",
            L"ASM_Template",
            OwningComp->GetOwner()->GetName().ToWideString(),
            ScriptPath,
            ScriptDisplayName
        );
        if (!bSuccess) {
            UE_LOG(ELogLevel::Error, TEXT("Failed to create script from template"));
        }
    }
    
    if (AnimSM == nullptr)
    {
        AnimSM = FObjectFactory::ConstructObject<UAnimationStateMachine>(this);
    }
    NativeInitializeAnimation();
}

void UAnimInstance::GetProperties(TMap<FString, FString>& OutProperties) const
{
    // AnimSM : 만들어줌
    // -> CurrentState, CurrentAnimationSequence, Transitions를 여기서 저장
    if(AnimSM)
		AnimSM->GetProperties(OutProperties);
    
	OutProperties.Add(TEXT("UAnimInstance::ScriptPath"), ScriptPath);
	OutProperties.Add(TEXT("UAnimInstance::ScriptDisplayName"), ScriptDisplayName);

    OutProperties.Add(TEXT("UAnimInstance::CurrentPose"), CurrentPose.ToString());
	if(PrevSequence)
		OutProperties.Add(TEXT("UAnimInstance::PrevSequence"), PrevSequence->GetSeqName());
	if(Sequence)
		OutProperties.Add(TEXT("UAnimInstance::Sequence"), Sequence->GetSeqName());
	OutProperties.Add(TEXT("UAnimInstance::PrevTime"), FString::Printf("%f", PrevTime));
	OutProperties.Add(TEXT("UAnimInstance::NextTime"), FString::Printf("%f", NextTime));
	OutProperties.Add(TEXT("UAnimInstance::BlendDuration"), FString::Printf("%f", BlendDuration));
	OutProperties.Add(TEXT("UAnimInstance::BlendElapsed"), FString::Printf("%f", BlendElapsed));
    OutProperties.Add(TEXT("UAnimInstance::NotifyQueue"), NotifyQueue.ToString());
    OutProperties.Add(TEXT("UAnimInstance::ActiveAnimNotifyState"), ActiveAnimNotifyState.ToString());
    OutProperties.Add(TEXT("UAnimInstance::CurrentTime"), FString::Printf(TEXT("%f"), CurrentTime));
    OutProperties.Add(TEXT("UAnimInstance::bPlaying"), FString::Printf(TEXT("%d"), bPlaying));
}

void UAnimInstance::SetProperties(const TMap<FString, FString>& InProperties)
{
    const FString* TempStr = nullptr;
    // ASM 생성
    if (AnimSM == nullptr)
    {
        AnimSM = FObjectFactory::ConstructObject<UAnimationStateMachine>(this);
    }
    AnimSM->SetProperties(InProperties);

	// CurrentPose
	TempStr = InProperties.Find(TEXT("UAnimInstance::ScriptPath"));
	if (TempStr)
	{
		ScriptPath = *TempStr;
	}
	TempStr = InProperties.Find(TEXT("UAnimInstance::ScriptDisplayName"));
	if (TempStr)
	{
		ScriptDisplayName = *TempStr;
	}



    // CurrentPose
    TempStr = InProperties.Find(TEXT("UAnimInstance::CurrentPose"));
    if (TempStr)
    {
        CurrentPose.InitFromString(*TempStr);
    }
    
    // Sequence
    TempStr = InProperties.Find(TEXT("UAnimInstance::PrevSequence"));
    if (TempStr)
    {
        if (UAnimSequenceBase* AnimSeq = FFbxManager::GetAnimSequenceByName(*TempStr))
        {
            PrevSequence = AnimSeq;
        }
        else
        {
            UE_LOG(ELogLevel::Warning, TEXT("SetProperties: AnimSequence '%s' not found"), **TempStr);
			PrevSequence = nullptr;
        }
    }
    TempStr = InProperties.Find(TEXT("UAnimInstance::Sequence"));
    if (TempStr)
    {
        if (UAnimSequenceBase* AnimSeq = FFbxManager::GetAnimSequenceByName(*TempStr))
        {
			Sequence = AnimSeq;
        }
        else
        {
            UE_LOG(ELogLevel::Warning, TEXT("SetProperties: AnimSequence '%s' not found"), **TempStr);
			Sequence = nullptr;
        }
    }

    // OwningComp는 USkeletalMeshComponent가 설정

	TempStr = InProperties.Find(TEXT("UAnimInstance::PrevTime"));
	if (TempStr)
	{
		PrevTime = FString::ToFloat(*TempStr);
	}
	TempStr = InProperties.Find(TEXT("UAnimInstance::NextTime"));
	if (TempStr)
	{
		NextTime = FString::ToFloat(*TempStr);
	}
	TempStr = InProperties.Find(TEXT("UAnimInstance::BlendDuration"));
	if (TempStr)
	{
		BlendDuration = FString::ToFloat(*TempStr);
	}
	TempStr = InProperties.Find(TEXT("UAnimInstance::BlendElapsed"));
	if (TempStr)
	{
		BlendElapsed = FString::ToFloat(*TempStr);
	}



    // NotifyQueue
    TempStr = InProperties.Find(TEXT("UAnimInstance::NotifyQueue"));
    if (TempStr)
    {
        NotifyQueue.InitFromString(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("UAnimInstance::ActiveAnimNotifyState"));
    if (TempStr)
    {
        ActiveAnimNotifyState.InitFromString(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("UAnimInstance::CurrentTime"));
    if (TempStr)
    {
        CurrentTime = FString::ToFloat(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("UAnimInstance::bPlaying"));
    if (TempStr)
    {
        bPlaying = FString::ToInt(*TempStr);
    }
}


void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
    // 업데이트 전 네이티브 먼저 실행
    NativeUpdateAnimation(DeltaSeconds);
    
    if (!bPlaying || !Sequence || !OwningComp || !OwningComp->GetSkeletalMesh())
    {
        // 재생 중이 아니거나, 필요한 정보가 없으면 현재 OutPose를 변경하지 않거나 참조 포즈로 설정
        ResetToRefPose();
        return;
    }

    // 사용자 확장 영역 - 커스텀 변수 또는 입력 값 설정
    TriggerAnimNotifies(DeltaSeconds);
    NotifyQueue.Reset();

    //// 1. 애니메이션 시간 업데이트
    //const float RateScale = Sequence->GetRateScale();
    //CurrentTime += DeltaSeconds * RateScale;

    // 트랜지션 준비
    PrepareTransition();

    if(AnimSM->GetTransitionState()){
        UpdateBlendTime(DeltaSeconds);
    }
    else {
        UpdateSingleAnimTime(DeltaSeconds);
    }
}

const TArray<FTransform>& UAnimInstance::EvaluateAnimation()
{
    USkeletalMesh* SkelMesh = OwningComp->GetSkeletalMesh();
    if (!SkelMesh || !Sequence)
    {
        ResetToRefPose();
        return CurrentPose;
    }

    FReferenceSkeleton RefSkeleton;
    SkelMesh->GetRefSkeleton(RefSkeleton);

    if (AnimSM->GetTransitionState() && PrevSequence) {
        EvaluateTransitionAnimation(RefSkeleton);
    }
    
    else {
        EvaluateSingleAnimation(RefSkeleton);
        
    }
    return CurrentPose;

}

void UAnimInstance::ResetToRefPose()
{
    if (OwningComp && OwningComp->GetSkeletalMesh())
    {
        FReferenceSkeleton RefSkeleton;
        OwningComp->GetSkeletalMesh()->GetRefSkeleton(RefSkeleton);
        CurrentPose = RefSkeleton.RawRefBonePose;
    }
}
USkeletalMeshComponent* UAnimInstance::GetSkelMeshComponent()
{
    return Cast<USkeletalMeshComponent>(OwningComp);
}

void UAnimInstance::SetCurrentTime(float NewTime)
{
    if (Sequence)
    {
        CurrentTime = FMath::Clamp(NewTime,0.f,Sequence->GetPlayLength());
    }
    else
    {
        UE_LOG(ELogLevel::Warning, TEXT("SetCurrentTime: Sequence is NULL"));
    }
}

void UAnimInstance::SetCurrentSequence(UAnimSequenceBase* NewSeq, float NewTime)
{
    AnimSM->SetCurrentAnimationSequence(NewSeq);
    Sequence = NewSeq;
    NextTime = NewTime;
}

UAnimationStateMachine* UAnimInstance::GetAnimSM()
{
    return AnimSM;
}

void UAnimInstance::PrepareTransition()
{
    if (!AnimSM->GetTransitionState()) {
        BlendElapsed = 0.f;
        PrevTime = CurrentTime;
        NextTime = 0.f;
        return;
    }

    FAnimTransition& PendingTransition = AnimSM->GetPendingTransition();
    
    PrevSequence = PendingTransition.FromState->GetLinkAnimationSequence();
    Sequence = PendingTransition.ToState->GetLinkAnimationSequence();
    BlendDuration = PendingTransition.Duration;
}

void UAnimInstance::UpdateBlendTime(float DeltaSeconds)
{
    BlendElapsed += DeltaSeconds;
    float BlendAlpha = BlendElapsed / BlendDuration;

    /** 이전 스퀀스가 없을 때 (Entry) : 블랜딩 종료 */
    if (PrevSequence == nullptr)
    {
        AnimSM->SetTransitionState(false);
        CurrentTime = NextTime;
        return;
    }
    
    // blend 완료 시점
    if (BlendAlpha >= 1.f) {
        AnimSM-> SetTransitionState(false);
        PrevSequence = nullptr;
        CurrentTime = NextTime;     // 다음 시퀀스로 전환된 시간 사용
    }
    else {
        // 전이 중에는 두 시퀀스 시간도 함게 갱신
        PrevTime += DeltaSeconds * (PrevSequence->GetRateScale());
        NextTime += DeltaSeconds * (Sequence->GetRateScale());
    }
}

void UAnimInstance::UpdateSingleAnimTime(float DeltaSeconds)
{
    if (Sequence) {
        CurrentTime += DeltaSeconds * Sequence->GetRateScale();

        // FIXING : 제거해도 되는 부분 확인하기
        if (UAnimSequenceBase* GetSequence = AnimSM->GetCurrentAnimationSequence())
        {
            if (Sequence != GetSequence)
            {
                CurrentTime = 0.0f;
            }

            Sequence = AnimSM->GetCurrentAnimationSequence();
            bPlaying = true;
        }

        const float SequenceLength = Sequence->GetPlayLength();
        const bool bLooping = Sequence->IsLooping();

        if (bLooping)
        {
            CurrentTime = FMath::Fmod(CurrentTime, SequenceLength);
            if (CurrentTime < 0.0f)
            {
                CurrentTime = SequenceLength - FMath::Fmod(-CurrentTime, SequenceLength);
            }
        }
        else // 루핑이 아닐 때
        {
            if (CurrentTime >= SequenceLength)
            {
                CurrentTime = SequenceLength; // 마지막 프레임에서 멈춤, 재생 종료
                SetPlaying(false);
            }
            else if (CurrentTime < 0.0f) // 음수 시간 처리 (RateScale이 음수일 경우)
            {
                CurrentTime = 0.0f;
                SetPlaying(false);
            }
        }
    }
}

void UAnimInstance::EvaluateTransitionAnimation(const FReferenceSkeleton& RefSkeleton)
{
    TArray<FTransform> PoseA, PoseB;
    UAnimDataModel* DataModelA = PrevSequence->GetDataModel();
    UAnimDataModel* DataModelB = Sequence->GetDataModel();
    DataModelA->GetPoseAtTime(PrevTime, PoseA, RefSkeleton, PrevSequence->IsLooping());
    DataModelB->GetPoseAtTime(NextTime, PoseB, RefSkeleton, Sequence->IsLooping());

    // 보간
    float Alpha = BlendElapsed / BlendDuration;
    CurrentPose.SetNum(PoseA.Num());
    for (int i = 0; i < PoseA.Num(); ++i)
        CurrentPose[i].Blend(PoseA[i], PoseB[i], Alpha);
}

void UAnimInstance::EvaluateSingleAnimation(const FReferenceSkeleton& RefSkeleton)
{
    if (CurrentPose.Num() != RefSkeleton.GetRawBoneNum())
    {
        CurrentPose.SetNum(RefSkeleton.GetRawBoneNum());
    }

    UAnimDataModel* DataModel = Sequence->GetDataModel();
    const float SequenceLength = Sequence->GetPlayLength();

    if (!DataModel || SequenceLength < 0.f)
    {
        ResetToRefPose();
        return;
    }

    DataModel->GetPoseAtTime(CurrentTime, CurrentPose, RefSkeleton, Sequence->IsLooping());
}

void UAnimInstance::InitializedLua()
{
    LuaState.open_libraries(sol::lib::base);
    SetLuaFunction();
    LoadStateMachineFromLua();
}

void UAnimInstance::LoadStateMachineFromLua()
{
    try {
        LuaState.script_file((*ScriptPath));
        bIsValidScript = true;
        const std::wstring FilePath = ScriptPath.ToWideString();
        LastWriteTime = std::filesystem::last_write_time(FilePath);
    }
    catch (const sol::error& err) {
        UE_LOG(ELogLevel::Error, TEXT("Lua Initialization error: %s"), err.what());
        return;
    }

    sol::table States = LuaState["States"];
    for (auto&& Pair : States.pairs())
    {
        auto tbl = Pair.second.as<sol::table>();
        
        FString Name = std::string(tbl["Name"].get<std::string>());
        FString AnimSequenceName = std::string(tbl["Sequence"].get<std::string>());

        UAnimSequence* NewSequence = FFbxManager::GetAnimSequenceByName(AnimSequenceName);
        UAnimNode_State* NewState = FObjectFactory::ConstructObject<UAnimNode_State>(AnimSM);
        NewState->Initialize(FName(Name), NewSequence);
        
        AnimSM->AddState(NewState);
    }

    sol::table Transitions = LuaState["Transitions"];
    for (auto&& Pair : Transitions.pairs())
    {
        auto tbl = Pair.second.as<sol::table>();

        FString From = std::string(tbl["From"].get<std::string>());
        FString To = std::string(tbl["To"].get<std::string>());
        sol::function Condition = tbl["Condition"];
        float Duration = tbl.get_or("Duration", 0.2f);

        AnimSM->AddTransition(FName(From), FName(To), [Condition]() { return Condition(); }, Duration);
    }

    AnimSM->SetState(FName("Entry"));
}

void UAnimInstance::SetLuaFunction()
{
    // Example
    // LuaState.set_function("IsKeyPressed", [&]() { return false; });
}

bool UAnimInstance::CheckFileModified()
{
    if (ScriptPath.IsEmpty())
    {
        return false;
    }

    try {
        std::wstring FilePath = ScriptPath.ToWideString();
        const auto FileTime = std::filesystem::last_write_time(FilePath);

        if (FileTime > LastWriteTime) {
            LastWriteTime = FileTime;
            return true;
        }
    }
    catch (const std::exception& e) {
        UE_LOG(ELogLevel::Error, TEXT("Failed to check lua script file"));
    }
    return false;
}

void UAnimInstance::ReloadScript()
{
    sol::table PersistentData;
    if (bIsValidScript && LuaState["PersistentData"].valid()) {
        PersistentData = LuaState["PersistentData"];
    }

    if (AnimSM)
    {
        AnimSM->ClearTransitions();
        AnimSM->ClearStates();
    }
    
    LuaState = sol::state();
    InitializedLua();

    if (PersistentData.valid()) {
        LuaState["PersistentData"] = PersistentData;
    }
}

void UAnimInstance::NativeInitializeAnimation()
{
}

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    if (AnimSM == nullptr)
    {
        return;
    }

    if (CheckFileModified()) {
        try {
            ReloadScript();
            UE_LOG(ELogLevel::Display, TEXT("Lua script reloaded"));
        }
        catch (const sol::error& e) {
            UE_LOG(ELogLevel::Error, TEXT("Failed to reload lua script"));
        }
    }

    AnimSM->ProcessState();

    if (UAnimSequenceBase* GetSequence = AnimSM->GetCurrentAnimationSequence())
    {
        if (Sequence != GetSequence)
        {
            CurrentTime = 0.0f;
        }
        
        Sequence = GetSequence;
        bPlaying = true; // 아마도 BeginPlay 시 호출해야할 듯?
    }
}

bool UAnimInstance::HandleNotify(const FAnimNotifyEvent& NotifyEvent)
{
    return false;
}

void UAnimInstance::TriggerSingleAnimNotify(const FAnimNotifyEvent& AnimNotifyEvent)
{
    if (HandleNotify(AnimNotifyEvent)) // 사용자가 AnimInstance단에서 오버라이딩한 경우 종료
    {
        return;
    }

    const float TriggerTime = AnimNotifyEvent.GetTriggerTime();
    const FName NotifyName = AnimNotifyEvent.NotifyName;

    if (OwningComp)
    {
        UE_LOG(ELogLevel::Display, TEXT("[Notify Triggered] Name: %s, TriggerTime: %.3f, OwningComp: VALID"), *NotifyName.ToString(), TriggerTime);

        ACharacter* Owner = Cast<ACharacter>(OwningComp->GetOwner());
        if (Owner)
        {
            UE_LOG(ELogLevel::Display, TEXT(" └ Owner: VALID (%s)"), *Owner->GetName());
            Owner->HandleAnimNotify(AnimNotifyEvent);
        }
        else
        {
            UE_LOG(ELogLevel::Warning, TEXT(" └ Owner: NULL"));
        }
    }
    else
    {
        UE_LOG(ELogLevel::Warning, TEXT("[Notify Triggered] Name: %s, TriggerTime: %.3f, OwningComp: NULL"), *NotifyName.ToString(), TriggerTime);
    }

}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
    if (!Sequence || !OwningComp)
    {
        return;
    }

    float PrevTime = CurrentTime - DeltaSeconds * Sequence->GetRateScale();
    float CurrTime = CurrentTime;

    TArray<FAnimNotifyEvent> NotifyStateBeginEvent;
    Sequence->GetAnimNotifies(PrevTime, DeltaSeconds, Sequence->IsLooping(), NotifyStateBeginEvent);
    NotifyQueue.AddAnimNotifies(NotifyStateBeginEvent);

    // 새롭게 활성화된 Anim Notify State 배열
    TArray<FAnimNotifyEvent> NewActiveAnimNotifyState;
    NewActiveAnimNotifyState.Reserve(NotifyQueue.AnimNotifies.Num());

    for (FAnimNotifyEvent& NotifyEvent : NotifyStateBeginEvent)
    {
        /* Duration 구간 내에 지속적으로 Notified 되는 State 유형 */
        if (NotifyEvent.IsStateNotify())
        {
            // Begin 처리: 이전 프레임에 없던 경우
            if (!ActiveAnimNotifyState.Contains(NotifyEvent))
            {
                NotifyEvent.NotifyStateClass->NotifyBegin(OwningComp, NotifyEvent.GetDuration());
            }

            // Tick 처리: 이미 활성화 Notify Event 배열에 존재하는 경우
            NotifyEvent.NotifyStateClass->NotifyTick(OwningComp, DeltaSeconds);

            NewActiveAnimNotifyState.Add(NotifyEvent);
            //NotifyQueue.AddStateNotify(NotifyEvent);
        }
        else
        {
            TriggerSingleAnimNotify(NotifyEvent);
        }
    }

    // End 처리: 이전에는 있었는데 이번에는 없는 Notify
    for (FAnimNotifyEvent& OldActive : ActiveAnimNotifyState)
    {
        if (!NewActiveAnimNotifyState.Contains(OldActive))
        {
            if (OldActive.IsStateNotify())
            {
                OldActive.NotifyStateClass->NotifyEnd(OwningComp);
            }

            if (!OwningComp || this->bPlaying == false)
            {
                UE_LOG(ELogLevel::Warning, TEXT("▶ While Notify End, AnimationInstance has been destroyed or ended "));
                return;
            }
        }
    }

    // 현재 활성 상태 업데이트
    ActiveAnimNotifyState = std::move(NewActiveAnimNotifyState);
}

