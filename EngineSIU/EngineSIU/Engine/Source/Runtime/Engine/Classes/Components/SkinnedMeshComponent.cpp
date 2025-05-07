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

    OutProperties["AssetPath"] = SkeletalMesh->Info.GetFullPath();
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
    TArray<FMatrix> OutBoneMatrices;

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
