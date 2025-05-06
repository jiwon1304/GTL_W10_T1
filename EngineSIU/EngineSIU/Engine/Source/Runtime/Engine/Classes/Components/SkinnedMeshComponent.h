#pragma once
#include "MeshComponent.h"


class USkinnedMeshComponent : public UMeshComponent
{
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

public:
    USkinnedMeshComponent() = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& Properties) override;

    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }
    void SetSkeletalMesh(USkeletalMesh* NewSkeletalMesh) { SkeletalMesh = NewSkeletalMesh; }

    TArray<FMatrix> CalculateBoneMatrices() const;

private:
    USkeletalMesh* SkeletalMesh = nullptr;
};
