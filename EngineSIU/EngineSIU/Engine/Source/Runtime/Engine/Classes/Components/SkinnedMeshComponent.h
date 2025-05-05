#pragma once
#include "MeshComponent.h"

struct FSkinnedMesh;

class USkinnedMeshComponent: public UMeshComponent
{
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)
public:
    USkinnedMeshComponent() = default;
    
    virtual UObject* Duplicate(UObject* InOuter) override;
    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;

    FSkinnedMesh* GetSkinnedMesh() const { return SkinnedMesh; }
    void SetSkinnedMesh(FSkinnedMesh* InSkinnedMesh);
protected:
    FSkinnedMesh* SkinnedMesh = nullptr;
};
