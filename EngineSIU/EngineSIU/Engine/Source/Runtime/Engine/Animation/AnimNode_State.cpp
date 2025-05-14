#include "AnimNode_State.h"
#include "Animation/AnimSequence.h"
#include "Engine/FbxManager.h"

void UAnimNode_State::Initialize(FName InStateName, UAnimSequenceBase* InLinkAnimationSequence)
{
    StateName = InStateName;
    LinkAnimationSequence = InLinkAnimationSequence;
}

UAnimSequenceBase* UAnimNode_State::GetLinkAnimationSequence() const
{
    return LinkAnimationSequence;
}

void UAnimNode_State::GetProperties(TMap<FString, FString>& OutProperties) const
{
    OutProperties.Add(TEXT("UAnimNode_State::StateName"), StateName.ToString());
    OutProperties.Add(TEXT("UAnimNode_State::LinkAnimationSequence"), LinkAnimationSequence ? LinkAnimationSequence->GetName() : TEXT("None"));
}

void UAnimNode_State::SetProperties(const TMap<FString, FString>& InProperties)
{
    const FString* TempStr = nullptr;
    TempStr = InProperties.Find(TEXT("UAnimNode_State::StateName"));
    if (TempStr)
    {
        StateName = FName(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("UAnimNode_State::LinkAnimationSequence"));
    if (TempStr)
    {
        if (*TempStr == TEXT("None"))
        {
            LinkAnimationSequence = nullptr;
        }
        else
        {
            LinkAnimationSequence = FFbxManager::GetAnimSequenceByName(*TempStr);
        }
    }
}
