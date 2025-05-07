#include "SkeletalMesh.h"

UObject* USkeletalMesh::Duplicate(UObject* InOuter)
{

    // TODO
    return nullptr;
}

uint32 USkeletalMesh::GetMaterialIndex(FString MaterialSlotName) const
{
    for (uint32 materialIndex = 0; materialIndex < Materials.Num(); materialIndex++) {
        if (Materials[materialIndex]->GetMaterialInfo().MaterialName == MaterialSlotName)
            return materialIndex;
    }

    return -1;
}

void USkeletalMesh::GetUsedMaterials(TArray<UMaterial*>& OutMaterial) const
{
    for (UMaterial* Material : Materials)
    {
        OutMaterial.Emplace(Material);
    }
}

FWString USkeletalMesh::GetOjbectName() const
{
    return RenderData->ObjectName;
}

void USkeletalMesh::SetData(FSkeletalMeshRenderData* InRenderData,
    FReferenceSkeleton InRefSkeleton, 
    TArray<FMatrix> InInverseBindPoseMatrices, 
    TArray<UMaterial*> InMaterials)
{
    RenderData = InRenderData;
    RefSkeleton = InRefSkeleton;
    InverseBindPoseMatrices = InInverseBindPoseMatrices;
    Materials = InMaterials;
    //for (const FSkelMeshRenderSection& Section : RenderData->RenderSections)
    //{
    //    DuplicateVerticesSection NewSection;
    //    NewSection.Vertices = Section.Vertices;
    //    DuplicatedVertices.Add(NewSection);
    //}
}
