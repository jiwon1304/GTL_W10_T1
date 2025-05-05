#include "SkinnedMeshComponent.h"
#include "Engine/FbxObject.h"

UObject* USkinnedMeshComponent::Duplicate(UObject* InOuter)
{
    return UMeshComponent::Duplicate(InOuter);
}

void USkinnedMeshComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    UMeshComponent::GetProperties(OutProperties);
}

void USkinnedMeshComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    UMeshComponent::SetProperties(InProperties);
}

void USkinnedMeshComponent::SetSkinnedMesh(FSkinnedMesh* InSkinnedMesh)
{
    SkinnedMesh = InSkinnedMesh;
    if (SkinnedMesh == nullptr)
    {
        AABB = FBoundingBox(FVector::ZeroVector, FVector::ZeroVector);
    } else
    {
        AABB = FBoundingBox(InSkinnedMesh->AABBmin, InSkinnedMesh->AABBmax);
    }
}
void USkinnedMeshComponent::CalculateBoneMatrices(TArray<FMatrix>& OutBoneMatrices) const
{
    if (!SkinnedMesh || SkinnedMesh->skeleton.joints.Num() == 0)
    {
        OutBoneMatrices.Add(FMatrix::Identity);
        return;
    }

    const TArray<FFbxJoint>& joints = SkinnedMesh->skeleton.joints;

    OutBoneMatrices.SetNum(joints.Num());

    for (int jointIndex = 0; jointIndex < joints.Num(); ++jointIndex)
    {
        const FFbxJoint& joint = joints[jointIndex];

        FMatrix ModelToBone = FMatrix::Identity;

        if (joint.parentIndex != -1)
        {
            // 부모의 ModelToBone * 내 BindPoseMatrix (row-vector 기준)
            ModelToBone = OutBoneMatrices[joint.parentIndex] * joint.localBindPose;

            if (jointIndex ==  14)
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
