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
    void SetAnimationAsset(UAnimSequenceBase* AnimToPlay, bool Cond)
    {
        Sequence = AnimToPlay;
        if (Sequence)
        {
            Sequence->SetName(Sequence->GetSeqName());
            Sequence->SetDataModel(Sequence->GetDataModel());
            Sequence->SetSequenceLength(Sequence->GetSequenceLength());
        }
    }

    void SetPlaying(bool bIsPlaying)
    {
        bPlaying = bIsPlaying;
    }
    void SetLooping(bool bIsLooping)
    {
        if (Sequence)
        {
            Sequence->bLoop = bIsLooping;
        }
    }

private:
    bool bPlaying = false;
};
