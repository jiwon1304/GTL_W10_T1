#include "Components/Mesh/SkeletalMeshRenderData.h"
#include "UObject/Casts.h"
#include "UObject/ObjectFactory.h"

#include "Engine/Asset/SkeletalMeshAsset.h"
#include "Engine/FFbxLoader.h"

UObject* USkinnedMesh::Duplicate(UObject* InOuter)
{
    // TODO: Context->CopyResource를 사용해서 Buffer 복사
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));
    return nullptr;
}

uint32 USkinnedMesh::GetMaterialIndex(FName MaterialSlotName) const
{
    for (uint32 materialIndex = 0; materialIndex < materials.Num(); materialIndex++) {
        if (materials[materialIndex]->MaterialSlotName == MaterialSlotName)
            return materialIndex;
    }

    return -1;
}

void USkinnedMesh::GetUsedMaterials(TArray<UMaterial*>& OutMaterial) const
{
    for (const FSkeletalMaterial* Material : materials)
    {
        OutMaterial.Emplace(Material->Material);
    }
}

FWString USkinnedMesh::GetObjectName() const
{
    return RenderData->ObjectName;
}

void USkinnedMesh::SetData(FSkeletalMeshRenderData* InRenderData)
{
    RenderData = InRenderData;

    for (int materialIndex = 0; materialIndex < RenderData->Materials.Num(); materialIndex++)
    {
        FSkeletalMaterial* newMaterialSlot = new FSkeletalMaterial();
        // UMaterial* newMaterial = FFbxManager::CreateMaterial(RenderData->Materials[materialIndex]);

        // newMaterialSlot->Material = newMaterial;
        // newMaterialSlot->MaterialSlotName = RenderData->Materials[materialIndex].MaterialName;
        //
        // materials.Add(newMaterialSlot);
    }
}
