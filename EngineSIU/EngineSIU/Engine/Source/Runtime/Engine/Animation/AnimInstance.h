#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class UAnimSequenceBase;

class UAnimInstance : public UObject
{
    DECLARE_CLASS(UAnimInstance, UObject)
public:
    UAnimInstance() = default;
    void TriggerAnimNotifies(float DeltaSeconds);


    UAnimSequenceBase* Sequence = nullptr; // 본래 FAnimNode_SequencePlayer에서 소유
};
