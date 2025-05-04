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
            if (USkeletalMesh* MeshToSet = FFbxManager::CreateSkeletalMesh(StringToWString(SkeletalMeshPath->ToAnsiString())))
            {
                SetSkeletalMesh(MeshToSet);
                UE_LOG(ELogLevel::Display, TEXT("Set SkeletalMesh '%s' for %s"), **SkeletalMeshPath, *GetName());
            }
            else
            {
                UE_LOG(ELogLevel::Warning, TEXT("Could not load SkeletalMesh '%s' for %s"), **SkeletalMeshPath, *GetName());
                SetSkeletalMesh(nullptr);
            }
        }
        else
        {
            SetSkeletalMesh(nullptr);
            UE_LOG(ELogLevel::Display, TEXT("Set SkeletalMesh to None for %s"), *GetName());
        }
    }
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* Value)
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

void USkeletalMeshComponent::CalculateBoneMatrices(TArray<FMatrix>& OutBoneMatrices) const
{
    if (!SkeletalMesh || SkeletalMesh->GetRenderData()->Bones.Num() == 0)
    {
        // No skeletal mesh or bones to process
        OutBoneMatrices.Add(FMatrix::Identity);
        return;
    }

    const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetRenderData();
    if (!RenderData)
    {
        return;
    }

    const TArray<FSkeletalMeshBone>& Bones = RenderData->Bones;
    const TArray<FMatrix>& RefPoseMatrices = RenderData->InverseBindPoseMatrices; // Bind pose matrices

    // Resize the output array to match the bone count
    OutBoneMatrices.SetNum(Bones.Num());

    // Start calculating bone matrices
    // 저장할 때 부모의 node부터 Bone array에 저장되므로 iteration으로 처리 가능.
    for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
    {
        const FSkeletalMeshBone& Bone = Bones[BoneIndex];

        // Combine the bone's local transform with its parent's transform
        FMatrix BoneMatrix = FMatrix::Identity;

        if (Bone.ParentIndex != -1) // If the bone has a parent
        {
            BoneMatrix = OutBoneMatrices[Bone.ParentIndex] * RefPoseMatrices[BoneIndex];
        }
        else // Root bone
        {
            BoneMatrix = RefPoseMatrices[BoneIndex];
        }

        // Store the final bone matrix
        OutBoneMatrices[BoneIndex] = BoneMatrix;
    }
}
