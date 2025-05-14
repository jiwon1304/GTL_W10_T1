#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class UAnimSequenceBase;

class UAnimNode_State : public UObject
{
    DECLARE_CLASS(UAnimNode_State, UObject)

public:
    UAnimNode_State() = default;
    std::function<UAnimSequenceBase* ()> GetLinkAnimationSequenceFunc;
    
public:
    void Initialize(FName InStateName, UAnimSequenceBase* InLinkAnimationSequence);

    uint32 GetStateName() const { return StateName.GetComparisonIndex(); }

    FName GetStateNameFName() const {
        return StateName;
    }
    UAnimSequenceBase* GetLinkAnimationSequence() const;

    void GetProperties(TMap<FString, FString>& OutProperties) const;
    void SetProperties(const TMap<FString, FString>& InProperties);
    
private:
    /** State Name */
    FName StateName;
    
    /** Link Animation */
    UAnimSequenceBase* LinkAnimationSequence;
};
