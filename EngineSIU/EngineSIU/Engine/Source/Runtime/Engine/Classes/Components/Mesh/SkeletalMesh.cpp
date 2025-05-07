#include "SkeletalMesh.h"

UObject* USkeletalMesh::Duplicate(UObject* InOuter)
{

    // TODO
    return nullptr;
}

uint32 USkeletalMesh::GetMaterialIndex(const FString& MaterialSlotName) const
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

FString USkeletalMesh::GetOjbectName() const
{
    return RenderData.ObjectName;
}

void USkeletalMesh::SetData(const FSkeletalMeshRenderData& InRenderData,
    const FReferenceSkeleton& InRefSkeleton,
    const TArray<FMatrix>& InInverseBindPoseMatrices,
    const TArray<UMaterial*>& InMaterials)
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

void USkeletalMesh::SerializeMesh(FArchive& Ar)
{
    Ar << RenderData
       << RefSkeleton
       << InverseBindPoseMatrices;

    for (UMaterial* Material : Materials)
    {
        Ar << Material->GetMaterialInfo();
    }
}
