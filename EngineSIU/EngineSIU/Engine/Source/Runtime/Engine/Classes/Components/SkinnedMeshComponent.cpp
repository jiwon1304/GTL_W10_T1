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
    OutBoneMatrices.Reserve(Bones.Num());

    for (const FBoneInfo& Bone : Bones)
    {
        FMatrix ModelToBone = FMatrix::Identity;
        if (Bone.ParentIndex != -1)
        {
            // 부모의 ModelToBone * 내 BindPoseMatrix (row-vector 기준)
            ModelToBone = OutBoneMatrices[Bone.ParentIndex] * Bone.LocalBindPoseMatrix;

            // if (BoneIdx == SelectedBoneIndex)
            // {
            //     FMatrix Translation = FMatrix::CreateTranslationMatrix(SelectedLocation);
            //     FMatrix Rotation = FMatrix::CreateRotationMatrix(SelectedRotation.Roll, SelectedRotation.Pitch, SelectedRotation.Yaw);
            //     FMatrix Scale = FMatrix::CreateScaleMatrix(SelectedScale.X, SelectedScale.Y, SelectedScale.Z);
            //
            //     ModelToBone = OutBoneMatrices[Bone.ParentIndex] * Bone.LocalBindPoseMatrix * Scale * Rotation * Translation;
            // }
        }
        else
        {
            ModelToBone = Bone.LocalBindPoseMatrix;
        }

        // 스키닝 행렬
        OutBoneMatrices.Add(ModelToBone * Bone.InverseBindPoseMatrix);
    }

    return OutBoneMatrices;
}
