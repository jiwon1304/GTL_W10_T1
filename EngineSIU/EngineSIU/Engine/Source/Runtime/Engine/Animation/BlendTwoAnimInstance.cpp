#include "BlendTwoAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "AnimData/AnimDataModel.h"
void UBlendTwoAnimInstance::UpdateAnimation(float DeltaSeconds, TArray<FTransform>& OutPose)
{
    if (!bPlaying || !SequenceA || !SequenceB || !OwningComp || !OwningComp->GetSkeletalMesh()) {
        if (OwningComp && OwningComp->GetSkeletalMesh())
        {
            FReferenceSkeleton RefSkeleton;
            OwningComp->GetSkeletalMesh()->GetRefSkeleton(RefSkeleton);

            if (OutPose.Num() != RefSkeleton.GetRawBoneNum()) {
                OutPose.SetNum(RefSkeleton.GetRawBoneNum());
            }
            for (int32 i = 0; i < RefSkeleton.RawRefBonePose.Num(); ++i) {
                OutPose[i] = RefSkeleton.RawRefBonePose[i];
            }
        }
        SetPlaying(false); // 재생 중지
        return;
    }

    UAnimDataModel* DataModelA = SequenceA->GetDataModel();
    UAnimDataModel* DataModelB = SequenceB->GetDataModel();

    
    if (!DataModelA || !DataModelB) {
        SetPlaying(false);
        return;
    }

    float LengthA = SequenceA->GetSequenceLength();
    float LengthB = SequenceB->GetSequenceLength();
    float RateScaleA = SequenceA->GetRateScale();
    float RateScaleB = SequenceB->GetRateScale();
    bool bLoopA = SequenceA->IsLooping();
    bool bLoopB = SequenceB->IsLooping();

    // 시간 업데이트 (A 기준으로 진행)
    CurrentTime += DeltaSeconds * RateScaleA;

    if (!bLoopA && CurrentTime >= LengthA) {
        CurrentTime = LengthA;
        SetPlaying(false);
    }

    // 포즈 추출
    USkeletalMesh* SkelMesh = OwningComp->GetSkeletalMesh();
    FReferenceSkeleton RefSkeleton;
    SkelMesh->GetRefSkeleton(RefSkeleton);

    const int32 NumBones = RefSkeleton.GetRawBoneNum();
    OutPose.SetNum(NumBones);

    TArray<FTransform> PoseA, PoseB;
    PoseA.SetNum(NumBones);
    PoseB.SetNum(NumBones);

    DataModelA->GetPoseAtTime(CurrentTime, PoseA, RefSkeleton, bLoopA);
    DataModelB->GetPoseAtTime(CurrentTime, PoseB, RefSkeleton, bLoopB);

    for (int32 i = 0; i < NumBones; ++i) {
        const FTransform& A = PoseA[i];
        const FTransform& B = PoseB[i];

        FTransform Blended;
        Blended.SetTranslation(FMath::Lerp(A.GetTranslation(), B.GetTranslation(), BlendAlpha));
        Blended.SetRotation(FQuat::Slerp(A.GetRotation(), B.GetRotation(), BlendAlpha));
        Blended.SetScale3D(FMath::Lerp(A.GetScale3D(), B.GetScale3D(), BlendAlpha));
        Blended.NormalizeRotation();

        OutPose[i] = Blended;
    }

}
