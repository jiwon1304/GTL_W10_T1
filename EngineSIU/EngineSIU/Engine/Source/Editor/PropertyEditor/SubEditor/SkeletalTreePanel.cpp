#include "SkeletalTreePanel.h"

#include "Animation/AnimSequence.h"
#include "Engine/Engine.h"
#include "Engine/EditorEngine.h"
#include "UObject/Object.h"
#include "Widgets/Layout/SSplitter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Contents/Actors/ItemActor.h"
#include "Engine/FbxManager.h"
#include "Engine/Asset/SkeletalMeshAsset.h"
#include "UnrealEd/ImGuiWidget.h"
#include "World/World.h"

void SkeletalTreePanel::Render()
{
    ImVec2 WinSize = ImVec2(Width, Height);
    
    /* TreeViewer */
    ImGui::SetNextWindowPos(ImVec2(WinSize.x * 0.75f + 2.0f, 2));
    ImGui::SetNextWindowSize(ImVec2(WinSize.x * 0.25f - 5.0f, WinSize.y * 0.7f - 5.0f));

    /* Flags */
    ImGuiWindowFlags PanelFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar;

    ImGui::Begin("Skeletal Tree", nullptr, PanelFlags);

    CreateSkeletalTreeNode();

    ImGui::End();

    /** Detail */
    ImGui::SetNextWindowPos(ImVec2(WinSize.x * 0.75f + 2.0f, WinSize.y * 0.7f + 2.0f));
    ImGui::SetNextWindowSize(ImVec2(WinSize.x * 0.25f - 5.0f, WinSize.y * 0.3f - 5.0f));

    ImGui::Begin("Detail", nullptr, PanelFlags);

    CreateSkeletalDetail();

    ImGui::End();
}

void SkeletalTreePanel::OnResize(HWND hWnd)
{
    if (hWnd != Handle)
    {
        return;
    }
    
    RECT ClientRect;
    GetClientRect(hWnd, &ClientRect);
    Width = static_cast<FLOAT>(ClientRect.right - ClientRect.left);
    Height = static_cast<FLOAT>(ClientRect.bottom - ClientRect.top);
}

void SkeletalTreePanel::RenderSkeletalTree(int InParentIndex, USkeletalMeshComponent* SkeletalMeshComponent, const TMap<int, FString>& BoneIndexToName)
{
    TArray<int> childrenIndices = SkeletalMeshComponent->GetChildrenOfBone(InParentIndex);

    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (childrenIndices.Num() == 0)
    {
        Flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (SkeletalMeshComponent->SelectedBoneIndex == InParentIndex)
    {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
    
    // boneIndexToName 맵에서 안전하게 이름 가져오기
    const FString* BoneNamePtr = BoneIndexToName.Find(InParentIndex);
    FString BoneName = BoneNamePtr ? *BoneNamePtr : FString::Printf(TEXT("Bone %d"), InParentIndex);

    // TreeNodeEx를 호출하기 전에 클릭 이벤트를 처리합니다.
    // TreeNodeEx가 클릭 이벤트를 소비하여 토글하는 것을 방지할 수 있습니다.
    bool bIsNodeClicked = false;
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) // 아이템이 클릭되었고, 토글(열기/닫기)되지 않은 경우
    {
        bIsNodeClicked = true;
    }
    
    bool NodeOpen = ImGui::TreeNodeEx(GetData(BoneName), Flags);

    if (bIsNodeClicked)
    {
        SkeletalMeshComponent->SelectedBoneIndex = InParentIndex;
        SelectedSkeleton = SkeletalMeshComponent;
        SelectedBoneIndex = InParentIndex;
    }

    if (NodeOpen)
    {
        for (int childIndex : childrenIndices)
        {
            RenderSkeletalTree(childIndex, SkeletalMeshComponent, BoneIndexToName);
        }
        ImGui::TreePop();
    }
}

void SkeletalTreePanel::CreateSkeletalTreeNode()
{
    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    if (!Engine)
    {
        SelectedSkeleton = nullptr;
        SelectedBoneIndex = -1;
        return;
    }

    // SkeletalMeshComponent 가져오기
    USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
    for (auto Actor : Engine->GetPreviewWorld(Handle)->GetActiveLevel()->Actors)
    {
        if (Actor && Actor->IsA<AItemActor>())
        {
            SkeletalMeshComponent = Cast<AItemActor>(Actor)->GetComponentByClass<USkeletalMeshComponent>();
            break;
        }
    }

    if (!SkeletalMeshComponent)
    {
        SelectedSkeleton = nullptr;
        SelectedBoneIndex = -1;
        return;
    }
    
    const TMap<int, FString> boneIndexToNameMap = SkeletalMeshComponent->GetBoneIndexToName();
    
    TArray<int> rootIndices = SkeletalMeshComponent->GetChildrenOfBone(-1); // 루트 본은 부모 인덱스가 -1인 경우가 일반적입니다.
    for (int rootIndex : rootIndices)
    {
        RenderSkeletalTree(rootIndex, SkeletalMeshComponent, boneIndexToNameMap);
    }
}

void SkeletalTreePanel::CreateSkeletalDetail()
{
    if (SelectedBoneIndex > -1 && SelectedSkeleton->SelectedBoneIndex > -1)
    {
        if (ImGui::Button("Reset Pose"))
        {
            SelectedSkeleton->ResetPose();
        }
        
        ImGui::Spacing();

        ImGui::Text("Bone Pose");
            
        FTransform& boneTransform = SelectedSkeleton->CurrentPose[SelectedSkeleton->SelectedBoneIndex];
        FImGuiWidget::DrawVec3Control("Location", boneTransform.Translation, 0, 85);
        ImGui::Spacing();

        FRotator Rotator = boneTransform.Rotation.Rotator();
        FRotator OriginalRotator = Rotator;
         FImGuiWidget::DrawRot3Control("Rotation", Rotator, 0, 85);
         FRotator Delta = Rotator - OriginalRotator;
         boneTransform.Rotate(Delta);

        ImGui::Spacing();

        FImGuiWidget::DrawVec3Control("Scale", boneTransform.Scale3D, 0, 85);

        ImGui::Dummy(ImVec2(0, 10));
        
        ImGui::Text("Reference Pose");
        
        FReferenceSkeleton skeleton;
        SelectedSkeleton->GetSkeletalMesh()->GetRefSkeleton(skeleton);
        FTransform refTransform = skeleton.RawRefBonePose[SelectedSkeleton->SelectedBoneIndex];
        FImGuiWidget::DrawVec3Control("RefLocation", refTransform.Translation, 0, 85);
        ImGui::Spacing();

        FRotator RefRotator = refTransform.Rotation.Rotator();
        FImGuiWidget::DrawRot3Control("RefRotation", RefRotator, 0, 85);
        refTransform.Rotation = FQuat(RefRotator);
        ImGui::Spacing();

        FImGuiWidget::DrawVec3Control("RefScale", refTransform.Scale3D, 0, 85);
    }
}
