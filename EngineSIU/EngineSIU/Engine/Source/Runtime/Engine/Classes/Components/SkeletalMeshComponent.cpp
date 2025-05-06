#include "SkeletalMeshComponent.h"

#include "Engine/AssetManager.h"
#include "Engine/FbxObject.h"
#include "Engine/FFbxLoader.h"
#include "UObject/Casts.h"

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
        FString PathFString = SkeletalMesh->name;
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
            if (UAssetManager::Get().AddAsset(StringToWString(SkeletalMeshPath->ToAnsiString())))
            {
                FSkeletalMesh* MeshToSet = FFbxLoader::GetFbxObject(SkeletalMeshPath->ToAnsiString());
                SetSkinnedMesh(MeshToSet);
                UE_LOG(ELogLevel::Display, TEXT("Set SkeletalMesh '%s' for %s"), **SkeletalMeshPath, *GetName());
            }
            else
            {
                UE_LOG(ELogLevel::Warning, TEXT("Could not load SkeletalMesh '%s' for %s"), **SkeletalMeshPath, *GetName());
                SetSkinnedMesh(nullptr);
            }
        }
        else
        {
            SetSkinnedMesh(nullptr);
            UE_LOG(ELogLevel::Display, TEXT("Set SkeletalMesh to None for %s"), *GetName());
        }
    }
}

void USkeletalMeshComponent::SetSkinnedMesh(FSkeletalMesh* InSkinnedMesh)
{
    SkeletalMesh = InSkinnedMesh;
    if (SkeletalMesh == nullptr)
    {
        AABB = FBoundingBox(FVector::ZeroVector, FVector::ZeroVector);
    } else
    {
        AABB = FBoundingBox(InSkinnedMesh->AABBmin, InSkinnedMesh->AABBmax);
    }
}
void USkeletalMeshComponent::CalculateBoneMatrices(TArray<FMatrix>& OutBoneMatrices) const
{
    if (!SkeletalMesh || SkeletalMesh->skeleton.joints.Num() == 0)
    {
        OutBoneMatrices.Add(FMatrix::Identity);
        return;
    }

    const TArray<FFbxJoint>& joints = SkeletalMesh->skeleton.joints;

    OutBoneMatrices.SetNum(joints.Num());

    for (int jointIndex = 0; jointIndex < joints.Num(); ++jointIndex)
    {
        const FFbxJoint& joint = joints[jointIndex];

        FMatrix ModelToBone = FMatrix::Identity;

        if (joint.parentIndex != -1)
        {
            // 부모의 ModelToBone * 내 BindPoseMatrix (row-vector 기준)
            ModelToBone = OutBoneMatrices[joint.parentIndex] * joint.localBindPose;

            if (jointIndex == -14)
            {
                static float deg = 0;
                deg += 5.f;

                // 어깨의 로컬축에서 회전(Rot * BindPoseMatrix)
                FMatrix Rot = FMatrix::CreateRotationMatrix(0, deg, -90);

                ModelToBone = OutBoneMatrices[joint.parentIndex] * joint.localBindPose * Rot;
            }
        }
        else
        {
            ModelToBone = joint.localBindPose;
        }

        // 스키닝 행렬: 
        const FMatrix& InverseBindPose = joint.inverseBindPose;
        OutBoneMatrices[jointIndex] = ModelToBone * InverseBindPose;
    }
}
