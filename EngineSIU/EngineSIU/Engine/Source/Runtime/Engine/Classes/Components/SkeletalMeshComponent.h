#pragma once
#include "Components/MeshComponent.h"
#include "Mesh/SkeletalMeshRenderData.h"
#include "Engine/Asset/SkeletalMeshAsset.h"

class USkeletalMeshComponent : public UMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, UMeshComponent)

public:
    USkeletalMeshComponent() = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;

    void SetSelectedBoneIndex(const int& Value) { SelectedBoneIndex = Value; }
    int GetSelectedBoneIndex() const { return SelectedBoneIndex; }

    virtual uint32 GetNumMaterials() const override;
    virtual UMaterial* GetMaterial(uint32 ElementIndex) const override;
    virtual uint32 GetMaterialIndex(FName MaterialSlotName) const override;
    virtual TArray<FName> GetMaterialSlotNames() const override;
    virtual void GetUsedMaterials(TArray<UMaterial*>& Out) const override;

    USkinnedMesh* GetSkeletalMesh() const { return SkeletalMesh; }
    void SetSkeletalMesh(USkinnedMesh* Value);

    void CalculateBoneMatrices(TArray<FMatrix>& OutBoneMatrices) const;

protected:
    USkinnedMesh* SkeletalMesh = nullptr;
    int SelectedBoneIndex = -1;
    TArray<UMaterial*> OverrideMaterials;
    FBoundingBox AABB = FBoundingBox(FVector::ZeroVector, FVector::ZeroVector);

};
