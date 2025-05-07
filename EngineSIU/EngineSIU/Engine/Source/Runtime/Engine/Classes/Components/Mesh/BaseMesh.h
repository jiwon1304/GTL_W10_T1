#pragma once
#include "EngineMeshTypes.h"
#include "Container/Array.h"
#include "Engine/AssetManager.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"


// 공통 메쉬 에셋 인터페이스
class UMeshAsset : public UObject
{
    DECLARE_CLASS(UMeshAsset, UObject)

public:
    UMeshAsset() = default;
    virtual ~UMeshAsset() override = default;

    void UpdateMaterials();

    FAssetInfo Info;                     // Asset의 정보
    TArray<UMaterial*> Materials;        // 이 메쉬에서 사용하는 머티리얼 목록
    TArray<FMaterialInfo> MaterialInfos; // FBX에서 불러와진 Raw Data
};
