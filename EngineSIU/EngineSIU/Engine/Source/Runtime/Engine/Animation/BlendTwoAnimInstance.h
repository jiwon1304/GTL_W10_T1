#pragma once

#include "AnimInstance.h"

class UAnimSequence;

class UBlendTwoAnimInstance : public UAnimInstance
{
public:
    UAnimSequenceBase* SequenceA;
    UAnimSequenceBase* SequenceB;

    float BlendAlpha = 0.0f;

protected:
    // 매 틱마다 애니메이션을 업데이트하고 최종 포즈를 OutPose에 반환합니다.
    virtual void UpdateAnimation(float DeltaSeconds, TArray<FTransform>& OutPose);
};
