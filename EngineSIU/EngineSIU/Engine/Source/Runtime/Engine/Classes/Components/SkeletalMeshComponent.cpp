#include "SkeletalMeshComponent.h"

#include "Engine/AssetManager.h"
#include "Engine/FbxObject.h"
#include "Engine/FFbxLoader.h"
#include "UObject/Casts.h"
#include "Components/Mesh/SkeletalMesh.h"

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
        FString PathFString = SkeletalMesh->GetOjbectName();
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
                USkeletalMesh* MeshToSet = FFbxLoader::GetFbxObject(SkeletalMeshPath->ToAnsiString());
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

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    SkeletalMesh = InSkeletalMesh;
}
void USkeletalMeshComponent::GetSkinningMatrices(TArray<FMatrix>& OutMatrices) const
{
    FReferenceSkeleton RefSkeleton;
    SkeletalMesh->GetRefSkeleton(RefSkeleton);
    if (!SkeletalMesh || RefSkeleton.RawNameToIndexMap.Num() == 0)
    {
        OutMatrices.Add(FMatrix::Identity);
        return;
    }

    const TArray<FTransform>& RefBonePose = RefSkeleton.RawRefBonePose;
    OutMatrices.SetNum(RefSkeleton.RawRefBonePose.Num());
    TArray<FMatrix> CurrentPoseMatrices; // joint -> model space
    CurrentPoseMatrices.SetNum(RefSkeleton.RawRefBonePose.Num());

    TArray<FMatrix> InverseBindPose;
    SkeletalMesh->GetInverseBindPoseMatrices(InverseBindPose);

    for (int JointIndex = 0; JointIndex < RefSkeleton.RawRefBonePose.Num(); ++JointIndex)
    {
        const FTransform& RefPose = RefBonePose[JointIndex];
        FMatrix BoneToModel = FMatrix::Identity;
        FMatrix LocalPose = FMatrix::CreateScaleMatrix(RefPose.Scale3D.X, RefPose.Scale3D.Y, RefPose.Scale3D.Z) *
            RefPose.Rotation.ToMatrix() *
            FMatrix::CreateTranslationMatrix(RefPose.Translation);

        int ParentIndex = RefSkeleton.RawRefBoneInfo[JointIndex].ParentIndex;

        // root node 또는 pelvis
        if (ParentIndex == -1)
        {
            BoneToModel = LocalPose;
        }
        else
        {
            BoneToModel = LocalPose * CurrentPoseMatrices[ParentIndex];
        }


        // Current pose 행렬 : j -> model space
        CurrentPoseMatrices[JointIndex] = BoneToModel;
        OutMatrices[JointIndex] = InverseBindPose[JointIndex] * BoneToModel;
    }
}

void USkeletalMeshComponent::GetCurrentPoseMatrices(TArray<FMatrix>& OutMatrices) const
{
    FReferenceSkeleton RefSkeleton;
    SkeletalMesh->GetRefSkeleton(RefSkeleton);
    if (!SkeletalMesh || RefSkeleton.RawNameToIndexMap.Num() == 0)
    {
        OutMatrices.Add(FMatrix::Identity);
        return;
    }

    const TArray<FTransform>& RefBonePose = RefSkeleton.RawRefBonePose;
    OutMatrices.SetNum(RefSkeleton.RawRefBonePose.Num());

    TArray<FMatrix> InverseBindPose;
    SkeletalMesh->GetInverseBindPoseMatrices(InverseBindPose);

    for (int JointIndex = 0; JointIndex < RefSkeleton.RawRefBonePose.Num(); ++JointIndex)
    {
        const FTransform& RefPose = RefBonePose[JointIndex];
        FMatrix BoneToModel = FMatrix::Identity;
        FMatrix LocalPose = FMatrix::CreateScaleMatrix(RefPose.Scale3D.X, RefPose.Scale3D.Y, RefPose.Scale3D.Z) *
            RefPose.Rotation.ToMatrix() *
            FMatrix::CreateTranslationMatrix(RefPose.Translation);

        int ParentIndex = RefSkeleton.RawRefBoneInfo[JointIndex].ParentIndex;

        // root node 또는 pelvis
        if (ParentIndex == -1)
        {
            BoneToModel = LocalPose;
        }
        else
        {
            BoneToModel = LocalPose * OutMatrices[ParentIndex];
        }


        // Current pose 행렬 : j -> model space
        OutMatrices[JointIndex] = BoneToModel;
    }



    //const TArray<FFbxJoint>& joints = SkeletalMesh->skeleton.joints;
    //OutMatrices.SetNum(joints.Num());

    //for (int jointIndex = 0; jointIndex < joints.Num(); ++jointIndex)
    //{
    //    const FFbxJoint& joint = joints[jointIndex];

    //    FMatrix ModelToBone = FMatrix::Identity;

    //    if (joint.parentIndex != -1)
    //    {
    //        if (jointIndex == SelectedBoneIndex)
    //        {

    //            FMatrix Translation = FMatrix::CreateTranslationMatrix(SelectedLocation);
    //            FMatrix Rotation = FMatrix::CreateRotationMatrix(SelectedRotation.Roll, SelectedRotation.Pitch, SelectedRotation.Yaw);
    //            FMatrix Scale = FMatrix::CreateScaleMatrix(SelectedScale.X, SelectedScale.Y, SelectedScale.Z);

    //            ModelToBone = Scale * Rotation * Translation * joint.localBindPose * OutMatrices[joint.parentIndex];
    //        }
    //        else
    //        {
    //            ModelToBone = joint.localBindPose * OutMatrices[joint.parentIndex]; // j->p(j)->model space
    //        }
    //    }
    //    else
    //    {
    //        ModelToBone = joint.localBindPose; // j -> model space
    //    }

    //    // 스키닝 행렬: 
    //    const FMatrix& InverseBindPose = joint.inverseBindPose;
    //    OutMatrices[jointIndex] = ModelToBone;
    //}
}

const TMap<int, FString> USkeletalMeshComponent::GetBoneIndexToName()
{
    TMap<int, FString> BoneIndexToName;
    BoneIndexToName.Add(-1, "None");
    FReferenceSkeleton RefSkeleton;
    SkeletalMesh->GetRefSkeleton(RefSkeleton);
    if (!SkeletalMesh || RefSkeleton.RawNameToIndexMap.Num() == 0)
    {
        return BoneIndexToName;
    }
    
    for (int i = 0 ; i < RefSkeleton.RawNameToIndexMap.Num(); ++i)
    {
        BoneIndexToName.Add(i, RefSkeleton.RawRefBoneInfo[i].Name);
    }
    return BoneIndexToName;
}
