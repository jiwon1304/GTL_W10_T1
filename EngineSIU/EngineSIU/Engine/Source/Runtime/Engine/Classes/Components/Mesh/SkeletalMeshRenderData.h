#pragma once

#include "Define.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/Material/Material.h"

struct FSkeletalMeshRenderData;

class USkinnedMesh : public UObject
{
    DECLARE_CLASS(USkinnedMesh, UObject)

public:
    USkinnedMesh() = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    const TArray<FSkeletalMaterial*>& GetMaterials() const { return materials; }
    uint32 GetMaterialIndex(FName MaterialSlotName) const;
    void GetUsedMaterials(TArray<UMaterial*>& OutMaterial) const;
    FSkeletalMeshRenderData* GetRenderData() const { return RenderData; }

    // ObjectName은 경로까지 포함
    FWString GetObjectName() const;

    void SetData(FSkeletalMeshRenderData* InRenderData);
private:
    FSkeletalMeshRenderData* RenderData = nullptr;
    TArray<FSkeletalMaterial*> materials;
};
