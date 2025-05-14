#include "AnimationStateMachine.h"

#include "AnimSequenceBase.h"
#include "Engine/FbxManager.h"

void UAnimationStateMachine::GetProperties(TMap<FString, FString>& OutProperties) const
{
    // USkeletalMeshComponent에서 호출됨.
    OutProperties.Add(TEXT("UAnimationStateMachine::CurrentState"), FString::Printf(TEXT("%d"), CurrentState));
    if (CurrentAnimationSequence)
    {
        OutProperties.Add(TEXT("UAnimationStateMachine::CurrentAnimationSequence"), CurrentAnimationSequence->GetSeqName());
    }
    else
    {
        OutProperties.Add(TEXT("UAnimationStateMachine::CurrentAnimationSequence"), TEXT("None"));
    }
    OutProperties.Add(TEXT("UAnimationStateMachine::Transitions"), Transitions.ToString());
}

void UAnimationStateMachine::SetProperties(const TMap<FString, FString>& InProperties)
{
    const FString* TempStr = nullptr;
    TempStr = InProperties.Find(TEXT("UAnimationStateMachine::CurrentState"));
    if (TempStr)
    {
        CurrentState = FString::ToInt(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("UAnimationStateMachine::CurrentAnimationSequence"));
    if (TempStr)
    {
        if (*TempStr != TEXT("None"))
        {
            CurrentAnimationSequence = FFbxManager::GetAnimSequenceByName(*TempStr);
        }
        else
        {
            CurrentAnimationSequence = nullptr;
        }
    }
    TempStr = InProperties.Find(TEXT("UAnimationStateMachine::Transitions"));
    Transitions.InitFromString(*TempStr);

}

void UAnimationStateMachine::AddTransition(UAnimNode_State* FromState, UAnimNode_State* ToState, const std::function<bool()>& Condition, float Duration)
{
    FAnimTransition NewTransition;
    NewTransition.FromState = FromState;
    NewTransition.ToState = ToState;
    NewTransition.Condition = Condition;
    NewTransition.Duration = Duration;

    Transitions.Add(NewTransition);
}

void UAnimationStateMachine::SetStateInternal(uint32 NewState)
{
    CurrentState = NewState;
}

void UAnimationStateMachine::SetState(FName NewStateName)
{
    uint32 ComparisonIndex = NewStateName.GetComparisonIndex();

    if (ComparisonIndex == 0)
    {
        return;
    }

    SetStateInternal(ComparisonIndex);
}

void UAnimationStateMachine::ProcessState()
{
    for (const auto& Transition : Transitions)
    {
        if (Transition.FromState->GetStateName() == CurrentState && Transition.Condition())
        {
            SetStateInternal(Transition.ToState->GetStateName());
            CurrentAnimationSequence = Transition.ToState->GetLinkAnimationSequence();
            break;
        }
    }
}

UAnimSequenceBase* UAnimationStateMachine::GetCurrentAnimationSequence() const
{
    return CurrentAnimationSequence;
}
