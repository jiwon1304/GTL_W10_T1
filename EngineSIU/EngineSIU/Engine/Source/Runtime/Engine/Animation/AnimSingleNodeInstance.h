#pragma once
#include "AnimInstance.h"
#include "Animation/AnimSequenceBase.h"


/*
 * 단일 애니메이션 재생하기 위한 경량 인스턴스
 */
class UAnimSingleNodeInstance : public UAnimInstance
{
    DECLARE_CLASS(UAnimSingleNodeInstance, UAnimInstance)
public:
    UAnimSingleNodeInstance() = default;
    void SetAnimationAsset(UAnimSequenceBase* AnimToPlay, bool Cond);

    virtual void NativeUpdateAnimation(float DeltaSeconds) override {}

    UAnimSequenceBase* GetCurrentSequence() const { return Sequence; }
};

inline void UAnimSingleNodeInstance::SetAnimationAsset(UAnimSequenceBase* AnimToPlay, bool bResetTimeAndState)
{
    if (Sequence == AnimToPlay && !bResetTimeAndState)
    {
        return; // 이미 같은 애니메이션며 리셋 안 함
    }

    Sequence = AnimToPlay;
    if (bResetTimeAndState)
    {
        CurrentTime = 0.0f; // 새 애니메이션 처음부터 시작
        // bPlaying 상태는 SetPlaying으로 외부에서 제어
    }

    if (Sequence)
    {
        
    }
    else
    {
        // 애니메이션이 없을 때는 애니메이션을 멈춤
        SetPlaying(false);
        CurrentTime = 0.0f;
    }
}
