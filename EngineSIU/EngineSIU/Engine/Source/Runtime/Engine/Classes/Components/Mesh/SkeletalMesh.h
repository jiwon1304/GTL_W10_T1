#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/Material/Material.h"
#include "Engine/Asset/SkeletalMeshAsset.h"
#include "Define.h"

class USkeletalMesh : public UObject
{
    DECLARE_CLASS(USkeletalMesh, UObject)

public:
    USkeletalMesh() = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    const TArray<UMaterial*>& GetMaterials() const { return Materials; }
    uint32 GetMaterialIndex(FString MaterialSlotName) const;
    void GetUsedMaterials(TArray<UMaterial*>& OutMaterial) const;
    const FSkeletalMeshRenderData& GetRenderData() const { return RenderData; }
    void GetRefSkeleton(FReferenceSkeleton& OutRefSkeleton) const
    {
        OutRefSkeleton = RefSkeleton;
    }
    void GetInverseBindPoseMatrices(TArray<FMatrix>& OutMatrices) const
    {
        OutMatrices = InverseBindPoseMatrices;
    }
    //void GetDuplicatedVerticesSection(TArray<FSkeletalVertex>& OutDuplicatedVertices, int32 SectionIndex) const
    //{
    //    OutDuplicatedVertices = DuplicatedVertices[SectionIndex];
    //}
    //void SetDuplicatedVerticesSection(const TArray<FSkeletalVertex>& InDuplicatedVertices, int32 SectionIndex)
    //{
    //    DuplicatedVertices[SectionIndex] = InDuplicatedVertices;
    //}

    //ObjectName은 경로까지 포함
    FString GetOjbectName() const;

    void SetData(FSkeletalMeshRenderData InRenderData,
        FReferenceSkeleton InRefSkeleton,
        TArray<FMatrix> InInverseBindPoseMatrices,
        TArray<UMaterial*> InMaterials);

    bool bCPUSkinned : 1 = false;
private:
    // TODO : UPROPERTIES
    // FbxLoader에 저장되어있음
    // 이건 수정하면 안됨(CPU Skinning에서도 이걸로 수정하면 안됨)
    FSkeletalMeshRenderData RenderData;

    // 로드 당시의 Skeleton로 초기화. Matrix는 매번 생성해서 사용
    FReferenceSkeleton RefSkeleton;

    // Inverse Bind Pose Matrix : const로 사용
    TArray<FMatrix> InverseBindPoseMatrices;

    // FSkeletalMaterial를 만들 이유가 없음...
    TArray<UMaterial*> Materials;
    
    //// CPU로 Skinning할 경우 수정할 수 있는 Vertex를 사용
    //typedef TArray<FSkeletalVertex> DuplicateVerticesSection;
    //TArray<DuplicateVerticesSection> DuplicatedVertices;

};
