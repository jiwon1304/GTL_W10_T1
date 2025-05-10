#include "SkeletalMeshComponent.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/AssetManager.h"
#include "Engine/FbxObject.h"
#include "Engine/FFbxLoader.h"
#include "UObject/Casts.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Math/JungleMath.h"
#include "UObject/ObjectFactory.h"

void USkeletalMeshComponent::InitializeComponent()
{
    Super::InitializeComponent();

    /* ImGui로 play 버튼 누르면 editor에서도 움직이도록 수정 */
    GetOwner()->SetActorTickInEditor(true);
}

void USkeletalMeshComponent::TickComponent(float DeltaSeconds)
{
    Super::TickComponent(DeltaSeconds);

    /* 애니메이션 비활성화 또는 필요한 에셋과 인스턴스 없으면 실행 안 함
     * 위 경우 CurrentPose는 이전 상태 유지하거나, ResetPose() 등으로 기본 포즈임
     */
    if (!bEnableAnimation || !SkeletalMesh || !AnimScriptInstance)
    {
        return;
    }

    AnimScriptInstance->UpdateAnimation(DeltaSeconds, CurrentPose);
}

UObject* USkeletalMeshComponent::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));
    NewComponent->SkeletalMesh = SkeletalMesh;
    NewComponent->SelectedBoneIndex = SelectedBoneIndex;
    NewComponent->ResetPose();
    return NewComponent;
}

void USkeletalMeshComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
        
    if (SkeletalMesh != nullptr)
    {
        FString PathFString = SkeletalMesh->GetOjbectName();
        OutProperties.Add(TEXT("SkeletalMeshPath"), PathFString);
    }
    else
    {
        OutProperties.Add(TEXT("SkeletalMeshPath"), TEXT("None"));
    }
}

void USkeletalMeshComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);

    const FString* SkeletalMeshPath = InProperties.Find(TEXT("SkeletalMeshPath"));
    if (SkeletalMeshPath)
    {
        if (*SkeletalMeshPath != TEXT("None"))
        {
            if (UAssetManager::Get().AddAsset(StringToWString(SkeletalMeshPath->ToAnsiString())))
            {
                USkeletalMesh* MeshToSet = FFbxLoader::GetSkeletalMesh(SkeletalMeshPath->ToAnsiString());
                SetSkeletalMesh(MeshToSet);
                UE_LOG(ELogLevel::Display, TEXT("Set SkeletalMesh '%s' for %s"), **SkeletalMeshPath, *GetName());
            }
            else
            {
                UE_LOG(ELogLevel::Warning, TEXT("Could not load SkeletalMesh '%s' for %s"), **SkeletalMeshPath, *GetName());
                SetSkeletalMesh(nullptr);
            }
        }
        else
        {
            SetSkeletalMesh(nullptr);
            UE_LOG(ELogLevel::Display, TEXT("Set SkeletalMesh to None for %s"), *GetName());
        }
    }
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    SkeletalMesh = InSkeletalMesh;
    SelectedBoneIndex = -1;
    ResetPose();
}
void USkeletalMeshComponent::GetSkinningMatrices(TArray<FMatrix>& OutMatrices) const
{
    if (!SkeletalMesh)
    {
        OutMatrices.Add(FMatrix::Identity);
        return;
    }
    
    FReferenceSkeleton RefSkeleton;
    SkeletalMesh->GetRefSkeleton(RefSkeleton);
    if (CurrentPose.Num() == 0)
    {
        OutMatrices.Add(FMatrix::Identity);
        return;
    }

    const TArray<FTransform>& BonePose = CurrentPose;
    OutMatrices.SetNum(CurrentPose.Num());
    TArray<FMatrix> CurrentPoseMatrices; // joint -> model space
    CurrentPoseMatrices.SetNum(CurrentPose.Num());

    TArray<FMatrix> InverseBindPose;
    SkeletalMesh->GetInverseBindPoseMatrices(InverseBindPose);

    for (int JointIndex = 0; JointIndex < CurrentPose.Num(); ++JointIndex)
    {
        const FTransform& RefPose = BonePose[JointIndex];
        FMatrix BoneToModel = FMatrix::Identity;
        FMatrix LocalPose = FMatrix::CreateScaleMatrix(RefPose.Scale3D.X, RefPose.Scale3D.Y, RefPose.Scale3D.Z) *
            RefPose.Rotation.ToMatrix() *
            FMatrix::CreateTranslationMatrix(RefPose.Translation);

        int ParentIndex = RefSkeleton.RawRefBoneInfo[JointIndex].ParentIndex;

        // root node 또는 pelvis
        if (ParentIndex == -1)
        {
            BoneToModel = LocalPose;
        }
        else
        {
            BoneToModel = LocalPose * CurrentPoseMatrices[ParentIndex];
        }


        // Current pose 행렬 : j -> model space
        CurrentPoseMatrices[JointIndex] = BoneToModel;
        OutMatrices[JointIndex] = InverseBindPose[JointIndex] * BoneToModel;
    }
}

void USkeletalMeshComponent::GetCurrentPoseMatrices(TArray<FMatrix>& OutMatrices) const
{
    if (!SkeletalMesh)
    {
        OutMatrices.Add(FMatrix::Identity);
        return;
    }
    
    FReferenceSkeleton RefSkeleton;
    SkeletalMesh->GetRefSkeleton(RefSkeleton);
    if (CurrentPose.Num() == 0)
    {
        OutMatrices.Add(FMatrix::Identity);
        return;
    }

    const TArray<FTransform>& BonePose = CurrentPose;
    OutMatrices.SetNum(RefSkeleton.RawRefBonePose.Num());

    TArray<FMatrix> InverseBindPose;
    SkeletalMesh->GetInverseBindPoseMatrices(InverseBindPose);

    for (int JointIndex = 0; JointIndex < RefSkeleton.RawRefBonePose.Num(); ++JointIndex)
    {
        const FTransform& RefPose = BonePose[JointIndex];
        FMatrix BoneToModel = FMatrix::Identity;
        FMatrix LocalPose = FMatrix::CreateScaleMatrix(RefPose.Scale3D.X, RefPose.Scale3D.Y, RefPose.Scale3D.Z) *
            RefPose.Rotation.ToMatrix() *
            FMatrix::CreateTranslationMatrix(RefPose.Translation);

        int ParentIndex = RefSkeleton.RawRefBoneInfo[JointIndex].ParentIndex;

        // root node 또는 pelvis
        if (ParentIndex == -1)
        {
            BoneToModel = LocalPose;
        }
        else
        {
            BoneToModel = LocalPose * OutMatrices[ParentIndex];
        }


        // Current pose 행렬 : j -> model space
        OutMatrices[JointIndex] = BoneToModel;
    }



    //const TArray<FFbxJoint>& joints = SkeletalMesh->skeleton.joints;
    //OutMatrices.SetNum(joints.Num());

    //for (int jointIndex = 0; jointIndex < joints.Num(); ++jointIndex)
    //{
    //    const FFbxJoint& joint = joints[jointIndex];

    //    FMatrix ModelToBone = FMatrix::Identity;

    //    if (joint.parentIndex != -1)
    //    {
    //        if (jointIndex == SelectedBoneIndex)
    //        {

    //            FMatrix Translation = FMatrix::CreateTranslationMatrix(SelectedLocation);
    //            FMatrix Rotation = FMatrix::CreateRotationMatrix(SelectedRotation.Roll, SelectedRotation.Pitch, SelectedRotation.Yaw);
    //            FMatrix Scale = FMatrix::CreateScaleMatrix(SelectedScale.X, SelectedScale.Y, SelectedScale.Z);

    //            ModelToBone = Scale * Rotation * Translation * joint.localBindPose * OutMatrices[joint.parentIndex];
    //        }
    //        else
    //        {
    //            ModelToBone = joint.localBindPose * OutMatrices[joint.parentIndex]; // j->p(j)->model space
    //        }
    //    }
    //    else
    //    {
    //        ModelToBone = joint.localBindPose; // j -> model space
    //    }

    //    // 스키닝 행렬: 
    //    const FMatrix& InverseBindPose = joint.inverseBindPose;
    //    OutMatrices[jointIndex] = ModelToBone;
    //}
}

TArray<int> USkeletalMeshComponent::GetChildrenOfBone(int InParentIndex) const
{
    if (SkeletalMesh == nullptr)
        return TArray<int>();
    FReferenceSkeleton refBones;
    SkeletalMesh->GetRefSkeleton(refBones);
    TArray<FMeshBoneInfo>& rawRefBones = refBones.RawRefBoneInfo;
    // TODO: FMeshBoneInfo에 children Index 저장해서 접근하기.
    TArray<int> result;
    for (int i = std::max(InParentIndex, 0); i < rawRefBones.Num(); ++i)
    {
        if (rawRefBones[i].ParentIndex == InParentIndex)
            result.Add(i);
    }
    return result;
}

const TMap<int, FString> USkeletalMeshComponent::GetBoneIndexToName()
{
    TMap<int, FString> BoneIndexToName;
    BoneIndexToName.Add(-1, "None");
    if (!SkeletalMesh)
        return BoneIndexToName;
    FReferenceSkeleton RefSkeleton;
    SkeletalMesh->GetRefSkeleton(RefSkeleton);
    if (RefSkeleton.RawNameToIndexMap.Num() == 0)
        return BoneIndexToName;
    
    for (int i = 0 ; i < RefSkeleton.RawNameToIndexMap.Num(); ++i)
    {
        BoneIndexToName.Add(i, RefSkeleton.RawRefBoneInfo[i].Name);
    }
    return BoneIndexToName;
}

void USkeletalMeshComponent::ResetPose()
{
    CurrentPose.Empty();
    
    FReferenceSkeleton RefSkeleton;
    if (!SkeletalMesh)
        return;
    SkeletalMesh->GetRefSkeleton(RefSkeleton);
    if (RefSkeleton.RawNameToIndexMap.Num() == 0)
        return;
    const TArray<FTransform>& RefBonePose = RefSkeleton.RawRefBonePose;
    
    CurrentPose.SetNum(RefBonePose.Num());
    for (int i = 0; i < RefBonePose.Num(); ++i)
    {
        CurrentPose[i] = RefBonePose[i];
    }
}

int USkeletalMeshComponent::CheckRayIntersection(const FVector& InRayOrigin, const FVector& InRayDirection, float& OutHitDistance) const
{
    if (!AABB.Intersect(InRayOrigin, InRayDirection, OutHitDistance))
    {
        return 0;
    }
    if (SkeletalMesh == nullptr)
    {
        return 0;
    }

    OutHitDistance = FLT_MAX;

    int IntersectionNum = 0;

    if (!SkeletalMesh)
    {
        UE_LOG(ELogLevel::Warning, TEXT("SkeletalMesh is not bound"));
        return 0;
    }
    const FSkeletalMeshRenderData& RenderData = SkeletalMesh->GetRenderData();

    for (int i = 0; i < RenderData.RenderSections.Num(); ++i)
    {
        const FSkelMeshRenderSection& RenderSection = RenderData.RenderSections[i];

        const TArray<FSkeletalVertex>& Vertices = RenderSection.Vertices;
        const int32 VertexNum = Vertices.Num();
        if (VertexNum == 0)
        {
            return 0;
        }

        const TArray<UINT>& Indices = RenderSection.Indices;
        const int32 IndexNum = Indices.Num();
        const bool bHasIndices = (IndexNum > 0);

        int32 TriangleNum = bHasIndices ? (IndexNum / 3) : (VertexNum / 3);
        for (int32 i = 0; i < TriangleNum; i++)
        {
            int32 Idx0 = i * 3;
            int32 Idx1 = i * 3 + 1;
            int32 Idx2 = i * 3 + 2;

            if (bHasIndices)
            {
                Idx0 = Indices[Idx0];
                Idx1 = Indices[Idx1];
                Idx2 = Indices[Idx2];
            }

            // 각 삼각형의 버텍스 위치를 FVector로 불러옵니다.
            FVector v0 = FVector(Vertices[Idx0].Position.X, Vertices[Idx0].Position.Y, Vertices[Idx0].Position.Z);
            FVector v1 = FVector(Vertices[Idx1].Position.X, Vertices[Idx1].Position.Y, Vertices[Idx1].Position.Z);
            FVector v2 = FVector(Vertices[Idx2].Position.X, Vertices[Idx2].Position.Y, Vertices[Idx2].Position.Z);

            float HitDistance = FLT_MAX;
            if (IntersectRayTriangle(InRayOrigin, InRayDirection, v0, v1, v2, HitDistance))
            {
                OutHitDistance = FMath::Min(HitDistance, OutHitDistance);
                IntersectionNum++;
            }
        }
    }

    return IntersectionNum;
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetSingleNodeInstance() const
{
    return Cast<class UAnimSingleNodeInstance>(AnimScriptInstance);
}

void USkeletalMeshComponent::SetAnimation(UAnimSequenceBase* NewAnimToPlay)
{
    if (!bEnableAnimation)
    {
        UE_LOG(ELogLevel::Warning, TEXT("SetAnimation: Animation is currently disabled"));
        return;
    }

    // AnimationSingleNode 모드인지 확인 및 AnimScriptInstance 생성/가져오기
    if (AnimationMode != EAnimationMode::AnimationSingleNode)
    {
        // UE_LOG(ELogLevel::Warning, TEXT("SetAnimation called but AnimationMode is not AnimationSingleNode. Forcing mode."));
        SetAnimationMode(EAnimationMode::AnimationSingleNode); // 강제로 모드 변경
    }

    // SetAnimationMode 내부에서 AnimScriptInstance가 생성되도록 하거나, 여기서 생성
    if (!AnimScriptInstance) 
    {
        AnimScriptInstance = FObjectFactory::ConstructObject<UAnimSingleNodeInstance>(nullptr);
        if (AnimScriptInstance) 
        {
            AnimScriptInstance->Initialize(this); // UAnimInstance 초기화
        }
        else 
        {
            UE_LOG(ELogLevel::Error, TEXT("Failed to create AnimScriptInstance in SetAnimation for %s"), *GetName());
            return;
        }
    }


    UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
    if (SingleNodeInstance)
    {
        // 애니메이션 설정 (시간 및 상태 리셋)
        SingleNodeInstance->SetAnimationAsset(NewAnimToPlay, true); 
        // 기본적으로는 재생 중지 상태로 설정 (Play 함수로 시작)
        SingleNodeInstance->SetPlaying(false); 
        if (!NewAnimToPlay) 
        { 
            ResetPose();        // 애니메이션이 null이면 참조 포즈로
        }
    }
}

void USkeletalMeshComponent::SetAnimationMode(EAnimationMode::Type InAnimationMode)
{
    AnimationMode = InAnimationMode;
}

void USkeletalMeshComponent::PlayAnimation(class UAnimSequenceBase* NewAnimToPlay, bool bLooping)
{
    SetAnimationMode(EAnimationMode::AnimationSingleNode); // 현재 기본모드는 single node
    SetAnimation(NewAnimToPlay);
    Play(bLooping);
}

void USkeletalMeshComponent::Play(bool bLooping) const
{
    if (!bEnableAnimation)
    {
        UE_LOG(ELogLevel::Warning, TEXT("Play: Animation is currently disabled"));
        return;
    }

    UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
    if (SingleNodeInstance)
    {
        if (SingleNodeInstance->GetCurrentSequence())
        {
            SingleNodeInstance->SetPlaying(true);
            SingleNodeInstance->GetCurrentSequence()->SetLooping(bLooping);
        }
    }
    else
    {
        UE_LOG(ELogLevel::Warning, TEXT("Play: No animation sequence set in AnimSingleNodeInstance for %s."), *GetName());
    }
}
