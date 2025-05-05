#pragma once
#include "BaseMesh.h"
#include "UObject/ObjectMacros.h"


class USkeletalMesh : public UMeshAsset
{
    DECLARE_CLASS(USkeletalMesh, UMeshAsset)

public:
    USkeletalMesh() = default;
    virtual ~USkeletalMesh() override = default;

public:
    TArray<FSkinnedVertex> Vertices;
    TArray<uint32> Indices;
    TArray<FMeshSubset> Subsets;

    FSkeleton SkeletonData;
    TArray<FAnimationClip> AnimationClips; // 감지된 애니메이션 클립 목록
    bool bHasAnimationData = false; // 애니메이션 데이터 존재 유무 플래그
};
