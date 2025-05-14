#include "AnimTransitionInstance.h"
#include "Classes/Components/SkeletalMeshComponent.h"
#include "Classes/Components/Mesh/SkeletalMesh.h"
#include "Animation/AnimData/AnimDataModel.h"

void UAnimTransitonInstance::SetAnimationAsset(UAnimSequenceBase* FromSeq, float FromTime, UAnimSequenceBase* ToSeq, float InBlendTime)
{
    if (FromSeq) {
        From.Sequence = FromSeq;
        From.CurrentTime = FromTime;
        From.RateScale = FromSeq->GetRateScale();
    }

    if (ToSeq) {
        To.Sequence = ToSeq;
        To.CurrentTime = 0.0f;
        To.RateScale = ToSeq->GetRateScale();

        BlendTime = InBlendTime;
        ElapsedBlendTime = 0.0f;
        BlendAlpha = 0.0f;
        bIsBlending = true;
    }

    SetPlaying(true);
}

void UAnimTransitonInstance::UpdateAnimation(float DeltaSeconds)
{
    if (!IsPlaying() || !OwningComp || !OwningComp->GetSkeletalMesh()) {
        ResetToRefPose();
        return;
    }

    From.Advance(DeltaSeconds);
    To.Advance(DeltaSeconds);

    if (bIsBlending && BlendTime > 0.0f) {
        ElapsedBlendTime += DeltaSeconds;
        BlendAlpha = FMath::Clamp(ElapsedBlendTime / BlendTime, 0.0f, 1.0f);

        if (BlendAlpha >= 1.0f) {
            bIsBlending = false;
            SetPlaying(false);
        }
    }
    else {
        BlendAlpha = 1.0f;
    }
}

const TArray<FTransform>& UAnimTransitonInstance::EvaluateAnimation()
{
    USkeletalMesh* SkelMesh = OwningComp->GetSkeletalMesh();
    if (!SkelMesh) {
        ResetToRefPose();
        return CurrentPose;
    }

    FReferenceSkeleton RefSkeleton;
    SkelMesh->GetRefSkeleton(RefSkeleton);
    const int32 NumBones = RefSkeleton.GetRawBoneNum();
    PoseA.SetNum(NumBones);
    PoseB.SetNum(NumBones);
    CurrentPose.SetNum(NumBones);

    if (From.IsValid()) {
        From.Sequence->GetDataModel()->GetPoseAtTime(From.CurrentTime, PoseA, RefSkeleton, From.Sequence->IsLooping());
    }
    else {
        PoseA = RefSkeleton.RawRefBonePose;
    }

    if (To.IsValid()) {
        To.Sequence->GetDataModel()->GetPoseAtTime(To.CurrentTime, PoseB, RefSkeleton, To.Sequence->IsLooping());
    }
    else {
        PoseB = RefSkeleton.RawRefBonePose;
    }

    for (int32 i = 0; i < NumBones; ++i) {
        CurrentPose[i].SetTranslation(FMath::Lerp(PoseA[i].GetTranslation(), PoseB[i].GetTranslation(), BlendAlpha));
        CurrentPose[i].SetRotation(FQuat::Slerp(PoseA[i].GetRotation(), PoseB[i].GetRotation(), BlendAlpha));
        CurrentPose[i].SetScale3D(FMath::Lerp(PoseA[i].GetScale3D(), PoseB[i].GetScale3D(), BlendAlpha));
    }

    return CurrentPose;
}

bool UAnimTransitonInstance::IsBlending() const
{
    return bIsBlending;
}
