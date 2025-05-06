#pragma once
#include "MeshComponent.h"

struct FSkeletalMesh;

class USkeletalMeshComponent: public UMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, UMeshComponent)
public:
    USkeletalMeshComponent() = default;
    
    virtual UObject* Duplicate(UObject* InOuter) override;
    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;

    FSkeletalMesh* GetSkinnedMesh() const { return SkeletalMesh; }
    void SetSkinnedMesh(FSkeletalMesh* InSkinnedMesh);
    void CalculateBoneMatrices(TArray<FMatrix>& OutBoneMatrices) const;
    void GetGlobalPoses(TArray<FMatrix>& OutMatrices) const;
    const TMap<int, FString>& GetBoneIndexToName();
    
    int SelectedBoneIndex = -1;
    FVector SelectedLocation;
    FRotator SelectedRotation;
    FVector SelectedScale = FVector::OneVector;
protected:
    FSkeletalMesh* SkeletalMesh = nullptr;
    TMap<int, FString> BoneIndexToName;
};
