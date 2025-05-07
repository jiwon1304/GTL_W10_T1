#include "SkeletalTreePanel.h"
#include "Engine/Engine.h"
#include "Engine/EditorEngine.h"
#include "UObject/Object.h"
#include "Widgets/Layout/SSplitter.h"
#include "AssetViewer/AssetViewer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Contents/Actors/ItemActor.h"
#include "Engine/Asset/SkeletalMeshAsset.h"
#include "UnrealEd/ImGuiWidget.h"
#include "World/World.h"

void SkeletalTreePanel::Render()
{
    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    if (!Engine)
    {
        return;
    }

    /* Pre Setup */
    // Splitter 기반 영역 계산
    SAssetViewer* AssetViewer = GEngineLoop.GetAssetViewer();
    FRect SkeletalTreeRect{ 0,0,0,0 };
    if (AssetViewer && AssetViewer->PrimaryVSplitter && AssetViewer->RightSidebarHSplitter->SideLT)
    {
        SkeletalTreeRect = AssetViewer->PrimaryVSplitter->SideLT->GetRect();
    }

    float PanelPosX = SkeletalTreeRect.TopLeftX;
    float PanelPosY = SkeletalTreeRect.TopLeftY + 72.f;
    float PanelWidth = SkeletalTreeRect.Width;
    float PanelHeight = SkeletalTreeRect.Height - 72.f -32.f;

    ImVec2 MinSize(50, 50);
    ImVec2 MaxSize(FLT_MAX, FLT_MAX);

    /* Min, Max Size */
    ImGui::SetNextWindowSizeConstraints(MinSize, MaxSize);

    /* Panel Position */
    ImGui::SetNextWindowPos(ImVec2(PanelPosX, PanelPosY), ImGuiCond_Always);

    /* Panel Size */
    ImGui::SetNextWindowSize(ImVec2(PanelWidth, PanelHeight), ImGuiCond_Always);

    /* Panel Flags */
    ImGuiWindowFlags PanelFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("Skeletal Tree", nullptr, PanelFlags);

    CreateSkeletalTreeNode();

    ImGui::End();
}

void SkeletalTreePanel::OnResize(HWND hWnd)
{
    RECT ClientRect;
    GetClientRect(hWnd, &ClientRect);
    Width = static_cast<FLOAT>(ClientRect.right - ClientRect.left);
    Height = static_cast<FLOAT>(ClientRect.bottom - ClientRect.top);
}

void SkeletalTreePanel::CreateSkeletalTreeNode()
{
    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    if (!Engine)
    {
        return;
    }

    // SkeletalMeshComponent 가져오기
    USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
    for (auto Actor : Engine->EditorPreviewWorld->GetActiveLevel()->Actors)
    {
        if (Actor && Actor->IsA<AItemActor>())
        {
            SkeletalMeshComponent = Cast<AItemActor>(Actor)->GetComponentByClass<USkeletalMeshComponent>();
            break;
        }
    }

    if (!SkeletalMeshComponent)
    {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("ModifyBone", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        ImGui::Text("Bone");

        const TMap<int, FString> boneIndexToName = SkeletalMeshComponent->GetBoneIndexToName();
        std::function<void(int)> CreateNode = [&CreateNode, &SkeletalMeshComponent, &boneIndexToName](int InParentIndex)
            {
                TArray<int> childrenIndices = SkeletalMeshComponent->GetChildrenOfBone(InParentIndex);

                ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_None;
                if (childrenIndices.Num() == 0)
                    Flags |= ImGuiTreeNodeFlags_Leaf;
                if (SkeletalMeshComponent->SelectedBoneIndex == InParentIndex)
                    Flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
                bool NodeOpen = ImGui::TreeNodeEx(GetData(boneIndexToName[InParentIndex]), Flags);

                if (ImGui::IsItemClicked())
                    SkeletalMeshComponent->SelectedBoneIndex = InParentIndex;
                if (NodeOpen)
                {
                    for (int childIndex : childrenIndices)
                    {
                        CreateNode(childIndex);
                    }
                    ImGui::TreePop();
                }
            };

        TArray<int> rootIndices = SkeletalMeshComponent->GetChildrenOfBone(-1);
        for (int rootIndex : rootIndices)
        {
            CreateNode(rootIndex);
        }

        if (SkeletalMeshComponent->SelectedBoneIndex > -1)
        {
            ImGui::Text("Bone Pose");
            ImGui::SameLine();
            if (ImGui::Button("Reset Pose"))
            {
                SkeletalMeshComponent->ResetPose();
            }
            FTransform& boneTransform = SkeletalMeshComponent->overrideSkinningTransform[SkeletalMeshComponent->SelectedBoneIndex];
            FImGuiWidget::DrawVec3Control("Location", boneTransform.Translation, 0, 85);
            ImGui::Spacing();

            FImGuiWidget::DrawRot3Control("Rotation", boneTransform.Rotation, 0, 85);
            ImGui::Spacing();

            FImGuiWidget::DrawVec3Control("Scale", boneTransform.Scale3D, 0, 85);

            ImGui::Text("Reference Pose");
            FReferenceSkeleton skeleton;
            SkeletalMeshComponent->GetSkeletalMesh()->GetRefSkeleton(skeleton);
            FTransform refTransform = skeleton.RawRefBonePose[SkeletalMeshComponent->SelectedBoneIndex];
            FImGuiWidget::DrawVec3Control("refLocation", refTransform.Translation, 0, 85);
            ImGui::Spacing();

            FImGuiWidget::DrawRot3Control("refRotation", refTransform.Rotation, 0, 85);
            ImGui::Spacing();

            FImGuiWidget::DrawVec3Control("refScale", refTransform.Scale3D, 0, 85);
        }
        //FVector& SelectedLocation = SkeletalMeshComponent->GetSkeletalMesh()->RefSkeleton.RawRefBonePose[SkeletalMeshComponent->SelectedBoneIndex].Translation;

        ImGui::Spacing();

        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}
