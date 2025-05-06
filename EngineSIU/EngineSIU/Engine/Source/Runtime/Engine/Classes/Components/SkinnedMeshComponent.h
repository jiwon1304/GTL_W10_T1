#pragma once
#include "MeshComponent.h"


class USkinnedMeshComponent : public UMeshComponent
{
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

public:
    USkinnedMeshComponent() = default;

    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }
    void SetSkeletalMesh(USkeletalMesh* NewSkeletalMesh) { SkeletalMesh = NewSkeletalMesh; }

private:
    USkeletalMesh* SkeletalMesh = nullptr;
};
