#include "SkinnedMeshComponent.h"
#include "Engine/FbxObject.h"

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
