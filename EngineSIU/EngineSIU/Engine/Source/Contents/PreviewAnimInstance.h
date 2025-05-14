#pragma once
#pragma once
#include "Animation/AnimInstance.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"


class UPreviewAnimInstance : public UAnimInstance
{
    DECLARE_CLASS(UPreviewAnimInstance, UAnimInstance)

public:
    UPreviewAnimInstance() = default;

    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;
    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;

    virtual void NativeInitializeAnimation() override;
    virtual void UpdateAnimation(float DeltaSeconds) override;

public:
    /** Hard Coding */
    UAnimSequenceBase* Anim1 = nullptr;
    UAnimSequenceBase* Anim2 = nullptr;
    UAnimSequenceBase* Anim3 = nullptr;


    // 0 : Idle->Walk 1 : Walk->Idle 2 : Walk -> Jump,  3 : Jump - > Walk, 4 : Idle->Jump , 5 : Jump->Idle
    TArray<uint8> bTransition = {false, false, false, false, false, false};
};
