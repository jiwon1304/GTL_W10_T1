#pragma once
#include "MeshComponent.h"
#include "Engine/Asset/SkeletalMeshAsset.h"

class USkeletalMesh;

class USkeletalMeshComponent: public UMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, UMeshComponent)
public:
    USkeletalMeshComponent() = default;
    
    virtual UObject* Duplicate(UObject* InOuter) override;
    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;

    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }
    void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
    void GetSkinningMatrices(TArray<FMatrix>& OutMatrices) const;
    void GetCurrentPoseMatrices(TArray<FMatrix>& OutMatrices) const;
    TArray<int> GetChildrenOfBone(int InParentIndex) const;
    const TMap<int, FString> GetBoneIndexToName();
    void ResetPose();
    
    int SelectedBoneIndex = -1;
    TArray<FTransform> overrideSkinningTransform;
    
protected:
    USkeletalMesh* SkeletalMesh = nullptr;
};
