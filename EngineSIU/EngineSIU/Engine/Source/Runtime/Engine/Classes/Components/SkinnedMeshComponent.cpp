#include "SkinnedMeshComponent.h"

#include "Mesh/SkeletalMesh.h"
#include "UObject/Casts.h"


UObject* USkinnedMeshComponent::Duplicate(UObject* InOuter)
{
    if (ThisClass* NewObject = Cast<ThisClass>(Super::Duplicate(InOuter)))
    {
        NewObject->SkeletalMesh = SkeletalMesh;

        return NewObject;
    }
    return nullptr;
}

void USkinnedMeshComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    UMeshComponent::GetProperties(OutProperties);

    OutProperties["AssetPath"] = SkeletalMesh ? SkeletalMesh->Info.GetFullPath() : FString{};
}

void USkinnedMeshComponent::SetProperties(const TMap<FString, FString>& Properties)
{
    UMeshComponent::SetProperties(Properties);

    if (USkeletalMesh* Mesh = FEngineLoop::ResourceManager.GetSkeletalMesh(Properties["AssetPath"]))
    {
        SkeletalMesh = Mesh;
    }
    else
    {
        UE_LOG(ELogLevel::Error, "Invalid AssetPath");
    }
}

TArray<FMatrix> USkinnedMeshComponent::CalculateBoneMatrices() const
{
    TArray<FMatrix> OutBoneMatrices; // 최종 스키닝 행렬을 담을 배열

    if (!SkeletalMesh || SkeletalMesh->SkeletonData.Bones.Num() == 0)
    {
        OutBoneMatrices.Add(FMatrix::Identity);
        return OutBoneMatrices;
    }

    const TArray<FBoneInfo>& Bones = SkeletalMesh->SkeletonData.Bones;

    OutBoneMatrices.Reserve(Bones.Num()); // 크기 미리 설정
    for (const FBoneInfo& Bone : Bones)
    {
        FMatrix CurrentBoneGlobalTransform;
        if (Bone.ParentIndex != -1)
        {
            // 부모의 모델 공간 기준 변환 * 현재 뼈의 로컬 바인드 포즈 변환
            CurrentBoneGlobalTransform = Bone.LocalBindPoseMatrix * OutBoneMatrices[Bone.ParentIndex];
        }
        else // 루트 뼈
        {
            CurrentBoneGlobalTransform = Bone.LocalBindPoseMatrix;
        }
        OutBoneMatrices.Add(CurrentBoneGlobalTransform); // 현재 뼈의 모델 공간 기준 변환 저장
    }

    for (int32 Idx = 0; Idx < Bones.Num(); ++Idx)
    {
        OutBoneMatrices[Idx] = Bones[Idx].InverseBindPoseMatrix * OutBoneMatrices[Idx];
    }

    return OutBoneMatrices;
}
// 애니메이션 테스트 (현재 메쉬가 꼬이는 버그 있음)
// {
//     TArray<FMatrix> FinalBoneMatrices; // 최종 스키닝 행렬
//
//     // --- Static 변수를 사용한 임시 애니메이션 시간 관리 ---
//     static float CurrentAnimTime = 0.0f;
//     // DeltaTime을 알 수 없으므로 고정된 시간 증분 사용 (예: 60fps 가정)
//     // 실제로는 엔진 루프에서 DeltaTime을 가져와야 합니다.
//     constexpr float FixedDeltaTimeForTest = 1.0f / 120.0f;
//     // ----------------------------------------------------
//
//     // 메쉬 또는 뼈 데이터 없으면 항등 행렬 반환
//     if (!SkeletalMesh || SkeletalMesh->SkeletonData.Bones.IsEmpty() || SkeletalMesh->AnimationClips.IsEmpty())
//     {
//         const int32 NumBones = SkeletalMesh ? SkeletalMesh->SkeletonData.Bones.Num() : 1;
//         FinalBoneMatrices.Init(FMatrix::Identity, NumBones > 0 ? NumBones : 1);
//         return FinalBoneMatrices;
//     }
//
//     const FSkeleton& Skeleton = SkeletalMesh->SkeletonData;
//     const TArray<FBoneInfo>& Bones = Skeleton.Bones;
//     const int32 NumBones = Bones.Num();
//
//     // --- 애니메이션 클립 및 시간 업데이트 ---
//     const FAnimationClip& AnimClip = SkeletalMesh->AnimationClips[0]; // 첫 번째 클립 사용
//     const float AnimDuration = AnimClip.DurationSeconds;
//
//     // static 시간 업데이트
//     CurrentAnimTime += FixedDeltaTimeForTest;
//
//     // 루핑 처리 (Duration이 0보다 클 때만)
//     if (AnimDuration > KINDA_SMALL_NUMBER) // KINDA_SMALL_NUMBER는 0에 가까운 작은 양수
//     {
//         CurrentAnimTime = fmodf(CurrentAnimTime, AnimDuration);
//         // 또는 while 루프 사용 (더 안전할 수 있음):
//         // while (CurrentAnimTime >= AnimDuration)
//         // {
//         //     CurrentAnimTime -= AnimDuration;
//         // }
//     }
//     else
//     {
//         CurrentAnimTime = 0.0f; // Duration이 0이면 항상 시작 시간
//     }
//     // 이제 'CurrentAnimTime'을 보간에 사용합니다.
//     // --- 애니메이션 클립 및 시간 업데이트 끝 ---
//
//
//     FinalBoneMatrices.SetNum(NumBones);
//
//     TArray<FMatrix> GlobalBoneTransforms; // 각 뼈의 애니메이션된 모델 공간 기준 글로벌 변환
//     GlobalBoneTransforms.SetNum(NumBones);
//
//     // 1. 각 뼈의 애니메이션된 글로벌 변환 계산
//     for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
//     {
//         const FBoneInfo& CurrentBoneInfo = Bones[BoneIdx];
//         FMatrix CurrentLocalTransformMatrix;
//
//         // --- 시작: 애니메이션 데이터에서 현재 뼈의 로컬 변환 계산 ---
//         FVector AnimTranslation = FVector::ZeroVector;
//         FQuat AnimRotation = FQuat::Identity;
//         FVector AnimScale = FVector(1.0f);
//         bool bFoundTrack = false;
//
//         const FBoneAnimation* FoundTrack = nullptr;
//         for(const FBoneAnimation& Track : AnimClip.BoneTracks)
//         {
//             if (Track.BoneName == CurrentBoneInfo.Name)
//             {
//                 FoundTrack = &Track;
//                 bFoundTrack = true;
//                 break;
//             }
//         }
//
//         if (bFoundTrack && FoundTrack->Keyframes.Num() > 0)
//         {
//             const TArray<FKeyframe>& Keyframes = FoundTrack->Keyframes;
//             int32 PrevKeyIdx = -1;
//             int32 NextKeyIdx = -1;
//
//             // 시간(CurrentAnimTime)에 맞는 키프레임 인덱스 찾기
//             for(int32 k = 0; k < Keyframes.Num(); ++k)
//             {
//                 if (Keyframes[k].TimeSeconds >= CurrentAnimTime)
//                 {
//                     NextKeyIdx = k;
//                     break;
//                 }
//                 PrevKeyIdx = k;
//             }
//
//             // 보간
//             if (PrevKeyIdx != -1 && NextKeyIdx != -1) // 두 키프레임 사이
//             {
//                 const FKeyframe& PrevKey = Keyframes[PrevKeyIdx];
//                 const FKeyframe& NextKey = Keyframes[NextKeyIdx];
//                 const float KeyDeltaTime = NextKey.TimeSeconds - PrevKey.TimeSeconds;
//                 // Alpha 계산 시 KeyDeltaTime이 0에 가까운 경우 처리
//                 const float Alpha = (KeyDeltaTime > KINDA_SMALL_NUMBER) ? FMath::Clamp((CurrentAnimTime - PrevKey.TimeSeconds) / KeyDeltaTime, 0.0f, 1.0f) : 0.0f;
//
//                 AnimTranslation = FMath::Lerp(PrevKey.Translation, NextKey.Translation, Alpha);
//                 AnimRotation = FQuat::Slerp(PrevKey.Rotation, NextKey.Rotation, Alpha);
//                 AnimScale = FMath::Lerp(PrevKey.Scale, NextKey.Scale, Alpha);
//             }
//             else if (NextKeyIdx != -1) // 첫 키프레임 이전/정확히 일치
//             {
//                  AnimTranslation = Keyframes[NextKeyIdx].Translation;
//                  AnimRotation = Keyframes[NextKeyIdx].Rotation;
//                  AnimScale = Keyframes[NextKeyIdx].Scale;
//             }
//             else // 마지막 키프레임 이후/정확히 일치
//             {
//                  AnimTranslation = Keyframes.Last().Translation; // 마지막 키 사용
//                  AnimRotation = Keyframes.Last().Rotation;
//                  AnimScale = Keyframes.Last().Scale;
//             }
//         }
//         // else: 애니메이션 트랙 없으면 기본 TRS 값(0, Identity, 1) 사용됨, 이후 바인드 포즈 행렬 사용 X
//
//         // 계산된 T, R, S로 로컬 변환 행렬 생성
//         if (bFoundTrack)
//         {
//             // (벡터를 왼쪽에 곱하는 M * v 시스템 기준, 언리얼 방식)
//             FMatrix TranslationMatrix = FMatrix::CreateTranslationMatrix(AnimTranslation);
//             FMatrix RotationMatrix = FMatrix::CreateRotationMatrix(AnimRotation.Rotator()); // FQuat에서 FRotator 거쳐 Matrix 생성 가정
//             FMatrix ScaleMatrix = FMatrix::CreateScaleMatrix(AnimScale);
//             // 행렬 곱셈 순서: Scale * Rotation * Translation (TRS)
//             CurrentLocalTransformMatrix = ScaleMatrix * RotationMatrix * TranslationMatrix;
//         }
//         else
//         {
//              // 애니메이션 트랙 없으면 로컬 바인드 포즈 사용
//             CurrentLocalTransformMatrix = CurrentBoneInfo.LocalBindPoseMatrix;
//         }
//         // --- 끝: 애니메이션 데이터에서 현재 뼈의 로컬 변환 계산 ---
//
//
//         // 글로벌 변환 계산 (언리얼 스타일: ParentGlobal * ChildLocal)
//         if (CurrentBoneInfo.ParentIndex != -1)
//         {
//             // 부모 인덱스가 현재 인덱스보다 작다고 가정 (정렬됨)
//             if (CurrentBoneInfo.ParentIndex < BoneIdx)
//             {
//                 // GlobalBoneTransforms[BoneIdx] = CurrentLocalTransformMatrix * GlobalBoneTransforms[CurrentBoneInfo.ParentIndex];
//                 GlobalBoneTransforms[BoneIdx] = CurrentLocalTransformMatrix * GlobalBoneTransforms[CurrentBoneInfo.ParentIndex];
//             }
//             else
//             {
//                 // 에러: 부모 인덱스가 현재 인덱스보다 크거나 같음 - 뼈 배열이 잘못 정렬됨
//                 // UE_LOG(YourLog, Error, TEXT("Bone array not sorted correctly! ParentIndex %d >= BoneIndex %d for bone %s"), CurrentBoneInfo.ParentIndex, BoneIdx, *CurrentBoneInfo.Name.ToString());
//                 GlobalBoneTransforms[BoneIdx] = CurrentLocalTransformMatrix; // 임시 처리
//             }
//         }
//         else // 루트 뼈
//         {
//             GlobalBoneTransforms[BoneIdx] = CurrentLocalTransformMatrix;
//         }
//     }
//
//     // 2. 최종 스키닝 행렬 계산 (언리얼 스타일: InverseBindPose * GlobalAnimated)
//     // 이 순서는 CPU 스키닝에서 V_skinned = M_skin * V_bind 라면, 셰이더에서 V_skinned = M_skin * V_bind 일 때와는 반대일 수 있습니다.
//     // 사용하는 스키닝 연산에 맞춰야 합니다. 언리얼 CPU 스키닝에 맞춘다면 이 순서가 맞습니다.
//     for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
//     {
//         FinalBoneMatrices[BoneIdx] = Bones[BoneIdx].InverseBindPoseMatrix * GlobalBoneTransforms[BoneIdx];
//     }
//
//     return FinalBoneMatrices;
// }
