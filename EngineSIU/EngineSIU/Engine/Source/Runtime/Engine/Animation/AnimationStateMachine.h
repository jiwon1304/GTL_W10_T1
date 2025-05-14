#pragma once
#include "AnimNode_State.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectFactory.h"
#include "Core/Misc/Parse.h"

class UAnimSequenceBase;

struct FAnimTransition
{
    UAnimNode_State* FromState = nullptr;
    UAnimNode_State* ToState = nullptr;

    /** Transition */
    int8 PriorityOrder = 1;
    std::function<bool()> Condition = [] {return false; };

    /** Blend Setting */
    float Duration = 0.2f;
    
    /** Transition condition */
    bool CanTransition() const
    {
        return Condition && Condition();
    }

    FString ToString() const
    {
        FString Result;
        Result += FString::Printf(TEXT("PriorityOrder=%d Duration=%f"), static_cast<int>(PriorityOrder), Duration);

        TMap<FString, FString> FromStateProperties;
        FromState->GetProperties(FromStateProperties);
        FString FromStateString = FromStateProperties.ToString();
        Result += FString::Printf(TEXT("FromState=%s"), *FromStateString);
        TMap<FString, FString> ToStateProperties;
        ToState->GetProperties(ToStateProperties);
        FString ToStateString = ToStateProperties.ToString();
        Result += FString::Printf(TEXT("ToState=%s"), *ToStateString);

        return Result;
    }

    bool InitFromString(const FString& InString)
    {
        // PriorityOrder 파싱
        int32 TmpPriorityOrder = PriorityOrder;
        FParse::Value(*InString, TEXT("PriorityOrder="), TmpPriorityOrder);
        PriorityOrder = static_cast<int8>(TmpPriorityOrder);

        // Duration 파싱
        float TmpDuration = Duration;
        FParse::Value(*InString, TEXT("Duration="), TmpDuration);
        Duration = TmpDuration;

        // FromState= 파싱 (ToState=가 나오기 전까지)
        FString FromStateMarker = TEXT("FromState=");
        FString ToStateMarker = TEXT("ToState=");

        int32 FromStateStart = InString.Find(FromStateMarker, ESearchCase::CaseSensitive);
        int32 ToStateStart = InString.Find(ToStateMarker, ESearchCase::CaseSensitive);

        if (FromStateStart == INDEX_NONE || ToStateStart == INDEX_NONE)
            return false;

        int32 FromStateValueStart = FromStateStart + FromStateMarker.Len();
        FString FromStateString = InString.Mid(FromStateValueStart, ToStateStart - FromStateValueStart);

        // ToState= 파싱 (끝까지)
        int32 ToStateValueStart = ToStateStart + ToStateMarker.Len();
        FString ToStateString = InString.Mid(ToStateValueStart);

        // TMap 파싱 함수 사용 (직접 구현 필요)
        TMap<FString, FString> FromStateProperties;
        TMap<FString, FString> ToStateProperties;

        FromStateProperties.InitFromString(FromStateString);
        ToStateProperties.InitFromString(ToStateString);

        // FromState/ToState 객체에 할당 (해당 타입에 맞게)
        if (!FromState)
        {
            FromState = FObjectFactory::ConstructObject<UAnimNode_State>(nullptr);
        }
        FromState->SetProperties(FromStateProperties);
        if (!ToState)
        {
            ToState = FObjectFactory::ConstructObject<UAnimNode_State>(nullptr);
        }
        ToState->SetProperties(ToStateProperties);
        return true;
    }

};


class UAnimationStateMachine : public UObject
{
    DECLARE_CLASS(UAnimationStateMachine, UObject)

public:
    UAnimationStateMachine() = default;

public:
    void GetProperties(TMap<FString, FString>& OutProperties) const;
    void SetProperties(const TMap<FString, FString>& InProperties);

    /**
     * Set the transition to FName.
     */
    void AddTransition(FName FromStateName, FName ToStateName, const std::function<bool()>& Condition, float Duration = 0.2f);

    /**
     * Set the transition to UAnimNode_State.
     */
    void AddTransition(UAnimNode_State* FromState, UAnimNode_State* ToState, const std::function<bool()>& Condition, float Duration = 0.2f);

    void AddState(UAnimNode_State* State);
    
    void SetState(FName NewStateName);

    void SetStateInternal(uint32 NewState);

    void ProcessState();

    void ClearTransitions();

    void ClearStates();

    FAnimTransition& GetPendingTransition() const { return *PendingTransition; }
    void SetPendingTransition(FAnimTransition* NewTransition) { PendingTransition = NewTransition; }
    
    /**
     * 
     * OutFrom : 현재 재생되고 있는 애님 시퀀스
     * OutTo : 변경될 애님 시퀀스
     */
    void GetAnimationsForBlending(UAnimSequenceBase*& OutFrom, UAnimSequenceBase*& OutTo) const;
    
    FORCEINLINE uint32 GetCurrentState() const { return CurrentState; }
    
    FORCEINLINE bool GetTransitionState() const { return bTransitionState; }
    
    FORCEINLINE void SetTransitionState(bool NewState) { bTransitionState = NewState; }
    
    UAnimSequenceBase* GetCurrentAnimationSequence() const;

    FORCEINLINE TMap<uint32, UAnimNode_State*>& GetStateContainer() { return StateContainer; }

    void SetCurrentAnimationSequence(UAnimSequenceBase* NewAnim) { CurrentAnimationSequence = NewAnim; }

    FORCEINLINE TArray<FAnimTransition>& GetTransitions() { return Transitions; }
   
private:
 
    /** FName comparison index by state name */
    uint32 CurrentState;

    /** Current playing animation sequence */
    UAnimSequenceBase* CurrentAnimationSequence = nullptr;

    /** Anim waiting for transfer */
    UAnimSequenceBase* FromAnimationSequence = nullptr;

    /** Transition list */
    TArray<FAnimTransition> Transitions;

    /** Is pending transition */
    FAnimTransition* PendingTransition;
    
    /** State Container */
    TMap<uint32, UAnimNode_State*> StateContainer;

    
    /** true when a state transition occurs */
    bool bTransitionState = false;
};
