#include "Components/SkeletalMeshComponent.h"
#include "Engine/FFbxLoader.h"
#include "UObject/Casts.h"
#include "UObject/ObjectFactory.h"
#include "GameFramework/Actor.h"

UObject* USkeletalMeshComponent::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));
    NewComponent->SkeletalMesh = SkeletalMesh;
    NewComponent->SelectedBoneIndex = SelectedBoneIndex;
    return NewComponent;
}

void USkeletalMeshComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);

    if (SkeletalMesh != nullptr)
    {
        FString PathFString = SkeletalMesh->GetObjectName().c_str();
        OutProperties.Add(TEXT("SkeletalMeshPath"), PathFString);
    }
    else
    {
        OutProperties.Add(TEXT("SkeletalMeshPath"), TEXT("None"));
    }
}

void USkeletalMeshComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);

    const FString* SkeletalMeshPath = InProperties.Find(TEXT("SkeletalMeshPath"));
    if (SkeletalMeshPath)
    {
        if (*SkeletalMeshPath != TEXT("None"))
        {
            // if (USkeletalMesh* MeshToSet = FFbxManager::CreateSkeletalMesh(StringToWString(SkeletalMeshPath->ToAnsiString())))
            // {
            //     SetSkeletalMesh(MeshToSet);
            //     UE_LOG(ELogLevel::Display, TEXT("Set SkeletalMesh '%s' for %s"), **SkeletalMeshPath, *GetName());
            // }
            // else
            // {
            //     UE_LOG(ELogLevel::Warning, TEXT("Could not load SkeletalMesh '%s' for %s"), **SkeletalMeshPath, *GetName());
            //     SetSkeletalMesh(nullptr);
            // }
        }
        else
        {
            SetSkeletalMesh(nullptr);
            UE_LOG(ELogLevel::Display, TEXT("Set SkeletalMesh to None for %s"), *GetName());
        }
    }
}

void USkeletalMeshComponent::SetSkeletalMesh(USkinnedMesh* Value)
{
    SkeletalMesh = Value;
    if (SkeletalMesh == nullptr)
    {
        OverrideMaterials.SetNum(0);
        AABB = FBoundingBox(FVector::ZeroVector, FVector::ZeroVector);
    }
    else
    {
        OverrideMaterials.SetNum(Value->GetMaterials().Num());
        // AABB = FBoundingBox(SkeletalMesh->GetRenderData()->BoundingBoxMin, SkeletalMesh->GetRenderData()->BoundingBoxMax);
    }
}

uint32 USkeletalMeshComponent::GetNumMaterials() const
{
    return SkeletalMesh ? SkeletalMesh->GetMaterials().Num() : 0;
}

UMaterial* USkeletalMeshComponent::GetMaterial(uint32 ElementIndex) const
{
    if (SkeletalMesh)
    {
        if (OverrideMaterials.IsValidIndex(ElementIndex) && OverrideMaterials[ElementIndex])
        {
            return OverrideMaterials[ElementIndex];
        }

        if (SkeletalMesh->GetMaterials().IsValidIndex(ElementIndex))
        {
            return SkeletalMesh->GetMaterials()[ElementIndex]->Material;
        }
    }
    return nullptr;
}

uint32 USkeletalMeshComponent::GetMaterialIndex(FName MaterialSlotName) const
{
    return SkeletalMesh ? SkeletalMesh->GetMaterialIndex(MaterialSlotName) : -1;
}

TArray<FName> USkeletalMeshComponent::GetMaterialSlotNames() const
{
    TArray<FName> MaterialNames;
    if (SkeletalMesh)
    {
        for (const FSkeletalMaterial* Material : SkeletalMesh->GetMaterials())
        {
            MaterialNames.Emplace(Material->MaterialSlotName);
        }
    }
    return MaterialNames;
}

void USkeletalMeshComponent::GetUsedMaterials(TArray<UMaterial*>& Out) const
{
    if (SkeletalMesh)
    {
        SkeletalMesh->GetUsedMaterials(Out);
        for (int32 MaterialIndex = 0; MaterialIndex < GetNumMaterials(); ++MaterialIndex)
        {
            if (OverrideMaterials.IsValidIndex(MaterialIndex) && OverrideMaterials[MaterialIndex])
            {
                Out[MaterialIndex] = OverrideMaterials[MaterialIndex];
            }
        }
    }
}
//
//void USkeletalMeshComponent::CalculateBoneMatrices(TArray<FMatrix>& OutBoneMatrices) const
//{
//    if (!SkeletalMesh || SkeletalMesh->GetRenderData()->Bones.Num() == 0)
//    {
//        // No skeletal mesh or bones to process
//        // 이제 무조건 Bone을 하나 붙여주고 있어서 안들어올듯...
//        OutBoneMatrices.Add(FMatrix::Identity);
//        return;
//    }
//
//    const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetRenderData();
//    if (!RenderData)
//    {
//        return;
//    }
//
//    const TArray<FSkeletalMeshBone>& Bones = RenderData->Bones;
//    const TArray<FMatrix>& InverseMatrices = RenderData->InverseBindPoseMatrices;
//
//    OutBoneMatrices.SetNum(Bones.Num());
//
//    for (int BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
//    {
//        const FSkeletalMeshBone& Bone = Bones[BoneIndex];
//
//        // model space -> Bone Space
//        FMatrix ModelToBone = FMatrix::Identity;
//
//        if (Bone.ParentIndex != -1)
//        {
//            // model -> 부모 bone의 space -> 내 bone의 space
//            // 원래 버텍스는 model space 기준 -> bone의 space로 변환
//            ModelToBone = OutBoneMatrices[Bone.ParentIndex] * Bone.BindPoseMatrix;
//            if (BoneIndex == 14)
//            {
//                static float rad = 0;
//                rad += 1.f;
//            
//                ModelToBone = Bone.BindPoseMatrix * FMatrix::CreateRotationMatrix(rad, 0, 0) * OutBoneMatrices[Bone.ParentIndex];
//            }
//        }
//        else
//        {
//            ModelToBone = Bone.BindPoseMatrix;
//        }
//
//
//        // Step 3: 스킨 매트릭스 계산: 현재 Pose * Inverse Bind Pose
//        // 
//        const FMatrix& InverseBindPose = InverseMatrices[BoneIndex];
//        OutBoneMatrices[BoneIndex] = ModelToBone * InverseBindPose;
//    }
//}

void USkeletalMeshComponent::CalculateBoneMatrices(TArray<FMatrix>& OutBoneMatrices) const
{
    if (!SkeletalMesh || SkeletalMesh->GetRenderData()->Bones.Num() == 0)
    {
        OutBoneMatrices.Add(FMatrix::Identity);
        return;
    }

    const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetRenderData();
    if (!RenderData)
    {
        return;
    }

    const TArray<FSkeletalMeshBone>& Bones = RenderData->Bones;
    const TArray<FMatrix>& InverseMatrices = RenderData->InverseBindPoseMatrices;

    OutBoneMatrices.SetNum(Bones.Num());

    for (int BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
    {
        const FSkeletalMeshBone& Bone = Bones[BoneIndex];

        FMatrix ModelToBone = FMatrix::Identity;

        if (Bone.ParentIndex != -1)
        {
            // 부모의 ModelToBone * 내 BindPoseMatrix (row-vector 기준)
            ModelToBone = OutBoneMatrices[Bone.ParentIndex] * Bone.BindPoseMatrix;

            if (BoneIndex ==  14)
            {
                static float deg = 0;
                deg += 5.f;

                // 어깨의 로컬축에서 회전(Rot * BindPoseMatrix)
                FMatrix Rot = FMatrix::CreateRotationMatrix(0, deg, -90);

                ModelToBone = OutBoneMatrices[Bone.ParentIndex] * Bone.BindPoseMatrix * Rot;
            }
        }
        else
        {
            ModelToBone = Bone.BindPoseMatrix;
        }

        // 스키닝 행렬: 
        const FMatrix& InverseBindPose = InverseMatrices[BoneIndex];
        OutBoneMatrices[BoneIndex] = ModelToBone * InverseBindPose;
    }
}
