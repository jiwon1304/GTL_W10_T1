#pragma once
#include "Engine/Asset/AnimationAsset.h"
#include "Engine/Animation/AnimInstance.h"
class UAnimTransitonInstance : public UAnimInstance
{
    DECLARE_CLASS(UAnimTransitonInstance, UAnimInstance)
public:
    UAnimTransitonInstance() = default;

    void SetAnimationAsset(UAnimSequenceBase* FromSeq, float FromTime, UAnimSequenceBase* ToSeq, float InBlendTime);

    virtual void UpdateAnimation(float DeltaSeconds) override;
    virtual const TArray<FTransform>& EvaluateAnimation() override;

    bool IsBlending() const;

protected:
    FBlendState From;
    FBlendState To;

    float BlendAlpha = 0.0f;
    float ElapsedBlendTime = 0.0f;
    float BlendTime = 0.2f;
    bool bIsBlending = false;

    TArray<FTransform> PoseA;
    TArray<FTransform> PoseB;
    TArray<FTransform> CurrentPose;
};
