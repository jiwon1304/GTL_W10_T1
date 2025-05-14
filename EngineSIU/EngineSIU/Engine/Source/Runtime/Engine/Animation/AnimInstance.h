#pragma once
#include <filesystem>

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "AnimNotifyQueue.h"
#include "sol/sol.hpp"

class UAnimationStateMachine;
struct FTransform;
class USkeletalMeshComponent;
class UAnimSequenceBase;
struct FReferenceSkeleton;
/*
 * 일반적인 애니메이션 블루프린트 기반의 인스턴스
 * 여러 개의 애니메이션을 소유하여 블렌드 및 상태 전이 가능 - 플레이어 캐릭터 등에 사용
 * 시퀀스를 오물딱 조물딱 블렌드 등을 하는 요리사
 * 즉, 애니메이션을 만들기 위해 필요한 데이터들의 집합
 */
class UAnimInstance : public UObject
{
    DECLARE_CLASS(UAnimInstance, UObject)
public:
    UAnimInstance();
    virtual void InitializeAnimation(USkeletalMeshComponent* InOwningComponent);

    virtual void GetProperties(TMap<FString, FString>& OutProperties) const;
    virtual void SetProperties(const TMap<FString, FString>& InProperties);

    // 매 틱마다 애니메이션을 업데이트하고 최종 포즈를 OutPose에 반환합니다.
    virtual void UpdateAnimation(float DeltaSeconds);
    virtual const TArray<FTransform>& EvaluateAnimation();
    void ResetToRefPose();

    virtual void NativeInitializeAnimation();
    virtual void NativeUpdateAnimation(float DeltaSeconds);

    /* 사용자가 오버라이딩할 Notify Handling 함수 입니다*/
    virtual bool HandleNotify(const FAnimNotifyEvent& NotifyEvent);
    void TriggerSingleAnimNotify(const FAnimNotifyEvent& AnimNotifyEvent);
    void TriggerAnimNotifies(float DeltaSeconds);

    void SetPlaying(bool bIsPlaying){ bPlaying = bIsPlaying;}
    bool IsPlaying() const { return bPlaying; }

    const TArray<FTransform>& GetCurrentPose() const { return CurrentPose; }
    
    USkeletalMeshComponent* GetSkelMeshComponent();
    
    float GetCurrentTime() const { return CurrentTime; }
    void SetCurrentTime(float NewTime);

    void SetCurrentSequence(UAnimSequenceBase* NewSeq, float NewTime);

    UAnimationStateMachine* GetAnimSM();

public:
    void InitializedLua();
    void LoadStateMachineFromLua();

    FString GetDisplayName() const { return ScriptDisplayName; }
    FString GetScriptPath() const { return ScriptPath; }
    
protected:
    virtual void SetLuaFunction();
    
protected:
    bool CheckFileModified();
    void ReloadScript();
    
protected:
    /** Lua */
    sol::state LuaState;
    FString ScriptPath;
    FString ScriptDisplayName;

    bool bIsValidScript = false;
    
    std::filesystem::file_time_type LastWriteTime;

protected:
    UAnimationStateMachine* AnimSM = nullptr;
    
    TArray<FTransform> CurrentPose;

    UAnimSequenceBase* PrevSequence = nullptr;
    UAnimSequenceBase* Sequence = nullptr; // 본래 FAnimNode_SequencePlayer에서 소유

    float PrevTime = 0.f;
    float NextTime = 0.f;

    float BlendDuration = 0.2f;
    float BlendElapsed = 0.f;

    USkeletalMeshComponent* OwningComp;

    FAnimNotifyQueue NotifyQueue;

    /* 현재 활성화 되어있는 Notify State 배열 */
    TArray<FAnimNotifyEvent> ActiveAnimNotifyState;

    float CurrentTime;
    bool bPlaying;
    //bool bIsBlend;

private:
    void PrepareTransition();
    void UpdateBlendTime(float DeltaSeconds);
    void UpdateSingleAnimTime(float DeltaSeconds);
    void EvaluateTransitionAnimation(const FReferenceSkeleton& RefSkeleton);
    void EvaluateSingleAnimation(const FReferenceSkeleton& RefSkeleton);   

};
