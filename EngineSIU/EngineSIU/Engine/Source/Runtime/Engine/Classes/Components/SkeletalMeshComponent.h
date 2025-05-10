#pragma once
#include "MeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/Asset/SkeletalMeshAsset.h"

class UAnimInstance;
class USkeletalMesh;

namespace EAnimationMode
{
    enum Type : int
    {
        AnimationBlueprint,
        AnimationSingleNode,
        // This is custom type, engine leaves AnimInstance as it is
        AnimationCustomMode,
    };
}

class USkeletalMeshComponent: public UMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, UMeshComponent)
public:
    USkeletalMeshComponent() = default;
    virtual void InitializeComponent() override;
    virtual void TickComponent(float DeltaSeconds) override;
    
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
    
    virtual int CheckRayIntersection(const FVector& InRayOrigin, const FVector& InRayDirection, float& OutHitDistance) const override;

    UAnimSingleNodeInstance* GetSingleNodeInstance() const;
    void SetAnimation(UAnimSequenceBase* NewAnimToPlay);
    void SetAnimationMode(EAnimationMode::Type AnimationSingleNode);
    void PlayAnimation(class UAnimSequenceBase* NewAnimToPlay, bool bLooping);
    void Play(bool bLooping) const;
public:
    int SelectedBoneIndex = -1;
    TArray<FTransform> CurrentPose;

protected:
    EAnimationMode::Type AnimationMode;
    uint8 bEnableAnimation : 1 = true;

protected:
    USkeletalMesh* SkeletalMesh = nullptr;
    UAnimInstance* AnimScriptInstance = nullptr;

    float CurrentAnimTime = 0.0f;
};
