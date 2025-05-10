#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/Material/Material.h"
#include "Define.h"

#include "Engine/Asset/StaticMeshAsset.h"

class UStaticMesh : public UObject
{
    DECLARE_CLASS(UStaticMesh, UObject)

public:
    UStaticMesh() = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    const TArray<FStaticMaterial*>& GetMaterials() const { return materials; }
    uint32 GetMaterialIndex(FName MaterialSlotName) const;
    void GetUsedMaterials(TArray<UMaterial*>& OutMaterial) const;
    const FStaticMeshRenderData& GetRenderData() const { return RenderData; }

    //ObjectName은 경로까지 포함
    FString GetOjbectName() const;

    void SetData(const FStaticMeshRenderData& InRenderData);

private:
    FStaticMeshRenderData RenderData;
    TArray<FStaticMaterial*> materials;
};
