#include "SkeletalMeshComponent.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/AssetManager.h"
#include "Engine/FbxObject.h"
#include "Engine/FbxManager.h"
#include "UObject/Casts.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Math/JungleMath.h"
#include "UObject/ObjectFactory.h"
#include "Animation/AnimTwoNodeBlendInstance.h"
#include "Animation/AnimTransitionInstance.h"
//#include "Animation/AnimationStateMachine.h"

void USkeletalMeshComponent::InitializeComponent()
{
    Super::InitializeComponent();

    /* ImGui로 play 버튼 누르면 editor에서도 움직이도록 수정 */
    GetOwner()->SetActorTickInEditor(true);

    //StateMachine = FObjectFactory::ConstructObject<UAnimationStateMachine>(nullptr);
}


void USkeletalMeshComponent::TickAnimation(float DeltaTime)
{
    /* 애니메이션 비활성화 또는 필요한 에셋과 인스턴스 없으면 실행 안 함
     * 위 경우 CurrentPose는 이전 상태 유지하거나, ResetPose() 등으로 기본 포즈임
     */
    if (!bEnableAnimation || !SkeletalMesh || !AnimScriptInstance)
    {
        return;
    }

    AnimScriptInstance->UpdateAnimation(DeltaTime);
    CurrentPose = AnimScriptInstance->EvaluateAnimation();
}


void USkeletalMeshComponent::TickComponent(float DeltaSeconds)
{
    Super::TickComponent(DeltaSeconds);
    TickAnimation(DeltaSeconds);
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

    // UAnimInstance는 USeketalMeshComponent와 1대1로 대응
    // 그 아래의 변수도 모두 unique함
    // 따라서 여기서 저장
    // 이후 UObject를 저장한다면 나중에 변경 필요.
    if (AnimScriptInstance)
    {
        OutProperties.Add(TEXT("USkeletalMeshComponent::AnimScriptInstance_ClassName"),
            AnimScriptInstance->GetClass()->GetName());
        AnimScriptInstance->GetProperties(OutProperties);
    }
    else
    {
        OutProperties.Add(TEXT("AnimScriptInstance"), TEXT("None"));
    }
        
    if (SkeletalMesh != nullptr)
    {
        FString PathFString = SkeletalMesh->GetObjectName();
        OutProperties.Add(TEXT("SkeletalMeshPath"), PathFString);
    }
    else
    {
        OutProperties.Add(TEXT("SkeletalMeshPath"), TEXT("None"));
    }

    OutProperties.Add(TEXT("CurrentPosePath"), CurrentPose.ToString());
    OutProperties.Add(TEXT("SelectedBoneIndex"), FString::Printf(TEXT("%d"), SelectedBoneIndex));
    OutProperties.Add(TEXT("AnimationMode"), FString::Printf(TEXT("%d"), AnimationMode));
    OutProperties.Add(TEXT("bEnableAnimation"), FString::Printf(TEXT("%d"), bEnableAnimation));
    OutProperties.Add(TEXT("CurrentAnimTime"), FString::Printf(TEXT("%f"), CurrentAnimTime));
}

void USkeletalMeshComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);

    const FString* SkeletalMeshPath = InProperties.Find(TEXT("SkeletalMeshPath"));
    if (SkeletalMeshPath)
    {
        if (*SkeletalMeshPath != TEXT("None"))
        {
            if (USkeletalMesh* MeshToSet = FFbxManager::GetSkeletalMesh(*SkeletalMeshPath))
            {
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

    const FString* TempStr = nullptr;
    if (!AnimScriptInstance)
    {
        TempStr = InProperties.Find(TEXT("USkeletalMeshComponent::AnimScriptInstance_ClassName"));
        if (TempStr)
        {
            UClass** AnimClass = UClass::GetClassMap().Find(*TempStr);
            if (AnimClass)
            {
                AnimScriptInstance = Cast<UAnimInstance>(FObjectFactory::ConstructObject(*AnimClass, this));
            }
            else
            {
                UE_LOG(ELogLevel::Warning, TEXT("Could not find AnimScriptInstance class '%s' for %s"), **TempStr, *GetName());
                return;
            }
        }
        if (AnimScriptInstance)
        {
            AnimScriptInstance->InitializeAnimation(this); // UAnimInstance 초기화
            AnimScriptInstance->SetProperties(InProperties);
        }
        else
        {
            UE_LOG(ELogLevel::Error, TEXT("Failed to create AnimScriptInstance in SetAnimation for %s"), *GetName());
            return;
        }
    }

    TempStr = InProperties.Find(TEXT("CurrentPosePath"));
    if (TempStr)
    {
        CurrentPose.InitFromString(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("SelectedBoneIndex"));
    if (TempStr)
    {
        SelectedBoneIndex = FString::ToInt(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("AnimationMode"));
    if (TempStr)
    {
        AnimationMode = static_cast<EAnimationMode::Type>(FString::ToInt(*TempStr));
    }
    TempStr = InProperties.Find(TEXT("bEnableAnimation"));
    if (TempStr)
    {
        bEnableAnimation = FString::ToInt(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("CurrentAnimTime"));
    if (TempStr)
    {
        CurrentAnimTime = FString::ToFloat(*TempStr);
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
        FMatrix LocalPose = RefPose.ToMatrixWithScale();

        int ParentIndex = RefSkeleton.RawRefBoneInfo[JointIndex].ParentIndex;

        // root node 또는 pelvis
        if (ParentIndex == -1)
        {
            BoneToModel = LocalPose;
        }
        else
        {
            BoneToModel = LocalPose * CurrentPoseMatrices[ParentIndex]; // colum major 기준?
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

UAnimTwoNodeBlendInstance* USkeletalMeshComponent::GetTwoNodeBlendInstance() const
{
    return Cast<class UAnimTwoNodeBlendInstance>(AnimScriptInstance);
}

UAnimTransitonInstance* USkeletalMeshComponent::GetTransitionInstance() const
{
    return TransitionInstance;
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
        UE_LOG(ELogLevel::Warning, TEXT("SetAnimation called but AnimationMode is not AnimationSingleNode. Forcing mode."));
        SetAnimationMode(EAnimationMode::AnimationSingleNode); // 강제로 모드 변경
    }
    
    switch (AnimationMode) {
    case EAnimationMode::AnimationSingleNode:
    {
        UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
        if (SingleNodeInstance == nullptr) {
            UE_LOG(ELogLevel::Error, TEXT("SkeletalMeshComp %s has no AnimScirptInstance"), *GetName());
            return;
        }
        SingleNodeInstance->SetAnimationAsset(NewAnimToPlay, true);
        SingleNodeInstance->SetPlaying(false);

        if (!NewAnimToPlay) {
            ResetPose();
        }
        break;
    }
    case EAnimationMode::AnimationTwoNodeBlend:
    {
        UAnimTwoNodeBlendInstance* TwoNodeBlendInstance = GetTwoNodeBlendInstance();
        if (TwoNodeBlendInstance == nullptr) {
            UE_LOG(ELogLevel::Error, TEXT("SkeletalMeshComp %s has no AnimScirptInstance"), *GetName());
            return;
        }
        TwoNodeBlendInstance->SetAnimationAsset(NewAnimToPlay, true, 1.f);
        TwoNodeBlendInstance->SetPlaying(false);
        break;
    }
    default:
        break;
    }
}

void USkeletalMeshComponent::SetAnimationMode(EAnimationMode::Type InAnimationMode)
{
    //FIXING : 현재 animscriptinstance delete 로직 구현
    /*if (AnimScriptInstance) {
        delete AnimScriptInstance;
        AnimScriptInstance = nullptr;
    }*/

    if (AnimationMode != InAnimationMode) {
        switch (InAnimationMode) {
        case EAnimationMode::AnimationSingleNode:
        {
            AnimScriptInstance = FObjectFactory::ConstructObject <UAnimSingleNodeInstance>(nullptr);
            break;
        }
        case EAnimationMode::AnimationTwoNodeBlend:
        {
            AnimScriptInstance = FObjectFactory::ConstructObject<UAnimTwoNodeBlendInstance>(nullptr);
            break;
        }
        case EAnimationMode::AnimationTransition:
            TransitionInstance = FObjectFactory::ConstructObject<UAnimTransitonInstance>(nullptr);
     
        default:
            break;
        }
        if (AnimScriptInstance) {
            AnimScriptInstance->InitializeAnimation(this);
        }
        else {
            UE_LOG(ELogLevel::Error, TEXT("Failed to create AnimScriptInstance in SetAnimation for %s"), *GetName());
            return;
        }
        AnimationMode = InAnimationMode;
    }
}

void USkeletalMeshComponent::PlayAnimation(class UAnimSequenceBase* NewAnimToPlay, bool bLooping)
{
    //SetAnimationMode(EAnimationMode::AnimationSingleNode); // 현재 기본모드는 single node
    SetAnimation(NewAnimToPlay);
    Play(bLooping);
}

void USkeletalMeshComponent::PlayTransitionAnimation(UAnimSequenceBase* FromSeq, float FromTime,
    UAnimSequenceBase* ToSeq, float BlendTime)
{
    SetAnimationMode(EAnimationMode::AnimationTransition);
    TransitionInstance->SetAnimationAsset(FromSeq, FromTime, ToSeq, BlendTime);
    TransitionInstance->SetPlaying(true);
}

void USkeletalMeshComponent::Play(bool bLooping) const
{
    if (!bEnableAnimation)
    {
        UE_LOG(ELogLevel::Warning, TEXT("Play: Animation is currently disabled"));
        return;
    }
    switch (AnimationMode) {
    case EAnimationMode::AnimationSingleNode:
    {
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
        break;
    }
    case EAnimationMode::AnimationTwoNodeBlend:
    {
        UAnimTwoNodeBlendInstance* TwoNodeBlendInstance = GetTwoNodeBlendInstance();
        if (TwoNodeBlendInstance) {
            TwoNodeBlendInstance->SetPlaying(true);
            if (TwoNodeBlendInstance->GetFromSequence()) {
                TwoNodeBlendInstance->GetFromSequence()->SetLooping(bLooping);

            }
            if (TwoNodeBlendInstance->GetToSequence()) {
                TwoNodeBlendInstance->GetToSequence()->SetLooping(bLooping);
            }
        }
        else
        {
            UE_LOG(ELogLevel::Warning, TEXT("Play: No animation sequence set in AnimTwoNodeBlendInstance for %s."), *GetName());
        }
        break;
    }
    default:
        break;
    }
    
}

void USkeletalMeshComponent::StopBlendAnimation()
{
    UAnimTwoNodeBlendInstance* BlendInstance = GetTwoNodeBlendInstance();
    if (BlendInstance) {
        BlendInstance->StopBlend(true);
    }
}

void USkeletalMeshComponent::SetAnimationInstance(UAnimInstance* NewAnimInstance)
{
    if (AnimScriptInstance != NewAnimInstance && NewAnimInstance != nullptr)
    {
        AnimScriptInstance = NewAnimInstance;
        NewAnimInstance->InitializeAnimation(this);
    }
}
