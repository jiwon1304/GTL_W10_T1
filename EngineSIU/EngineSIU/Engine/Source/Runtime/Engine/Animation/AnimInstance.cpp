#include "AnimInstance.h"

#include "AnimSequenceBase.h"
#include "AnimData/AnimDataModel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Engine/Asset/SkeletalMeshAsset.h"
#include "Math/Transform.h"

UAnimInstance::UAnimInstance()
    : OwningComp(nullptr)
    , Sequence(nullptr)
    , CurrentTime(0.0f)
    , bPlaying(false)
{
}

void UAnimInstance::Initialize(USkeletalMeshComponent* InOwningComponent)
{
    OwningComp = InOwningComponent;
}


void UAnimInstance::UpdateAnimation(float DeltaSeconds, TArray<FTransform>& OutPose)
{
    if (!bPlaying || !Sequence || !OwningComp || !OwningComp->GetSkeletalMesh())
    {
        // 재생 중이 아니거나, 필요한 정보가 없으면 현재 OutPose를 변경하지 않거나 참조 포즈로 설정
        if (OwningComp && OwningComp->GetSkeletalMesh()) 
        {
            FReferenceSkeleton RefSkeleton;
            OwningComp->GetSkeletalMesh()->GetRefSkeleton(RefSkeleton);
            if (OutPose.Num() != RefSkeleton.GetRawBoneNum()) 
            {
                OutPose.SetNum(RefSkeleton.GetRawBoneNum());
            }
            for (int32 i = 0; i < RefSkeleton.RawRefBonePose.Num(); ++i) 
            {
                OutPose[i] = RefSkeleton.RawRefBonePose[i];
            }
        }
        return;
    }


    UAnimDataModel* DataModel = Sequence->GetDataModel();
    const float SequenceLength = Sequence->GetSequenceLength();
    const float RateScale = Sequence->GetRateScale();
    const bool bLooping = Sequence->IsLooping();


    if (!DataModel || SequenceLength <= 0.0f)
    {
        // 유효한 데이터 모델이 없으면 참조 포즈 반환
        if (OwningComp && OwningComp->GetSkeletalMesh()) 
        {
            FReferenceSkeleton RefSkeleton;
            OwningComp->GetSkeletalMesh()->GetRefSkeleton(RefSkeleton);
            if (OutPose.Num() != RefSkeleton.GetRawBoneNum()) 
            {
                OutPose.SetNum(RefSkeleton.GetRawBoneNum());
            }
            for (int32 i = 0; i < RefSkeleton.GetRawBoneNum(); ++i) {
                OutPose[i] = RefSkeleton.RawRefBonePose[i];
            }
        }
        SetPlaying(false); // 재생 중지
        return;
    }

    // 1. 애니메이션 시간 업데이트
    CurrentTime += DeltaSeconds * RateScale;

    // 2. 루프 및 종료 처리
    if (bLooping)
    {
        if (CurrentTime >= SequenceLength)
        {
            CurrentTime = FMath::Fmod(CurrentTime, SequenceLength);
        }
        // 음수 시간 처리 (RateScale이 음수일 경우)
        else if (CurrentTime < 0.0f)
        {
            CurrentTime = SequenceLength - FMath::Fmod(-CurrentTime, SequenceLength);
            if (CurrentTime == SequenceLength) CurrentTime = 0.0f; // 정확히 0이 되도록
        }
    }
    else // 루핑이 아닐 때
    {
        if (CurrentTime >= SequenceLength)
        {
            CurrentTime = SequenceLength; // 마지막 프레임에서 멈춤
            SetPlaying(false);                    // 재생 종료
        }
        else if (CurrentTime < 0.0f)
        {
            CurrentTime = 0.0f;
            SetPlaying(false);
        }
    }


    // 3. 현재 시간에 맞는 포즈 가져오기
    USkeletalMesh* SkelMesh = OwningComp->GetSkeletalMesh();
    FReferenceSkeleton RefSkeleton;
    SkelMesh->GetRefSkeleton(RefSkeleton);

    if (OutPose.Num() != RefSkeleton.GetRawBoneNum())
    {
        OutPose.SetNum(RefSkeleton.GetRawBoneNum());
    }

    DataModel->GetPoseAtTime(CurrentTime, OutPose, RefSkeleton, bLooping);
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
    // TODO: 현재 시간과 이전 시간을 비교하여 Notify 트리거
}
