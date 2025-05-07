#include "BaseMesh.h"
#include "Engine/FObjLoader.h"


void UMeshAsset::UpdateMaterials()
{
    Materials.Empty(MaterialInfos.Num());

    for (const FMaterialInfo& MatInfo : MaterialInfos)
    {
        // TODO: 아니 왜 MaterialMap이 ResourceManager가 아니라 FObjManager에 있지?
        Materials.Add(FObjManager::CreateMaterial(MatInfo.ConvertObjMaterialInfo()));
    }
}
