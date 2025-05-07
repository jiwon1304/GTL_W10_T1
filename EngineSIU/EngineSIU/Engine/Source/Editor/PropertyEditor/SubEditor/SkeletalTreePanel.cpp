#include "SkeletalTreePanel.h"
#include "Engine/Engine.h"
#include "Engine/EditorEngine.h"
#include "UObject/Object.h"
#include "Widgets/Layout/SSplitter.h"
#include "AssetViewer/AssetViewer.h"

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

}
