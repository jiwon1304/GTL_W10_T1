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
void USkeletalMeshComponent::GetSkinningMatrices(TArray<FMatrix>& OutMatrices) const
{
    if (!SkeletalMesh || SkeletalMesh->skeleton.joints.Num() == 0)
    {
        OutMatrices.Add(FMatrix::Identity);
        return;
    }

    const TArray<FFbxJoint>& joints = SkeletalMesh->skeleton.joints;
    TArray<FMatrix> CurrentPose; // joint -> model space
    OutMatrices.SetNum(joints.Num());
    CurrentPose.SetNum(joints.Num());

    for (int jointIndex = 0; jointIndex < joints.Num(); ++jointIndex)
    {
        const FFbxJoint& joint = joints[jointIndex];

        FMatrix ModelToBone = FMatrix::Identity;

        if (joint.parentIndex != -1)
        {
            if (jointIndex == SelectedBoneIndex)
            {

                FMatrix Translation = FMatrix::CreateTranslationMatrix(SelectedLocation);
                FMatrix Rotation = FMatrix::CreateRotationMatrix(SelectedRotation.Roll, SelectedRotation.Pitch, SelectedRotation.Yaw);
                FMatrix Scale = FMatrix::CreateScaleMatrix(SelectedScale.X, SelectedScale.Y, SelectedScale.Z);

                ModelToBone = Scale * Rotation * Translation * joint.localBindPose * CurrentPose[joint.parentIndex];
            }
            else
            {
                ModelToBone = joint.localBindPose * CurrentPose[joint.parentIndex]; // j->p(j)->model space
            }
        }
        else
        {
            ModelToBone = joint.localBindPose; // j -> model space
        }

        // Current pose 행렬 : j -> model space
        const FMatrix& InverseBindPose = joint.inverseBindPose;
        CurrentPose[jointIndex] = ModelToBone;
        OutMatrices[jointIndex] = InverseBindPose * ModelToBone;
    }
}

void USkeletalMeshComponent::GetCurrentPoseMatrices(TArray<FMatrix>& OutMatrices) const
{
    if (!SkeletalMesh || SkeletalMesh->skeleton.joints.Num() == 0)
    {
        OutMatrices.Add(FMatrix::Identity);
        return;
    }

    const TArray<FFbxJoint>& joints = SkeletalMesh->skeleton.joints;
    OutMatrices.SetNum(joints.Num());

    for (int jointIndex = 0; jointIndex < joints.Num(); ++jointIndex)
    {
        const FFbxJoint& joint = joints[jointIndex];

        FMatrix ModelToBone = FMatrix::Identity;

        if (joint.parentIndex != -1)
        {
            if (jointIndex == SelectedBoneIndex)
            {

                FMatrix Translation = FMatrix::CreateTranslationMatrix(SelectedLocation);
                FMatrix Rotation = FMatrix::CreateRotationMatrix(SelectedRotation.Roll, SelectedRotation.Pitch, SelectedRotation.Yaw);
                FMatrix Scale = FMatrix::CreateScaleMatrix(SelectedScale.X, SelectedScale.Y, SelectedScale.Z);

                ModelToBone = Scale * Rotation * Translation * joint.localBindPose * OutMatrices[joint.parentIndex];
            }
            else
            {
                ModelToBone = joint.localBindPose * OutMatrices[joint.parentIndex]; // j->p(j)->model space
            }
        }
        else
        {
            ModelToBone = joint.localBindPose; // j -> model space
        }

        // 스키닝 행렬: 
        const FMatrix& InverseBindPose = joint.inverseBindPose;
        OutMatrices[jointIndex] = ModelToBone;
    }
}

const TMap<int, FString>& USkeletalMeshComponent::GetBoneIndexToName()
{
    BoneIndexToName.Empty();
    BoneIndexToName.Add(-1, "None");
    if (!SkeletalMesh || SkeletalMesh->skeleton.joints.Num() == 0)
    {
        return BoneIndexToName;
    }
    
    for (int i = 0 ; i < SkeletalMesh->skeleton.joints.Num(); ++i)
    {
        BoneIndexToName.Add(i, SkeletalMesh->skeleton.joints[i].name);
    }
    return BoneIndexToName;
}
