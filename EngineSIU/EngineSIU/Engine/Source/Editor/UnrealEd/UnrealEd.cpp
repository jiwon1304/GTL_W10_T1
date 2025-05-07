#include "UnrealEd.h"
#include "EditorPanel.h"

#include "PropertyEditor/ControlEditorPanel.h"
#include "PropertyEditor/OutlinerEditorPanel.h"
#include "PropertyEditor/PropertyEditorPanel.h"
#include "PropertyEditor/SubEditor/SkeletalTreePanel.h"
#include "PropertyEditor/SkeletalMeshControlPanel.h"

void UnrealEd::Initialize()
{
    auto ControlPanel = std::make_shared<ControlEditorPanel>();
    AddEditorPanel("ControlPanel", ControlPanel);

    auto OutlinerPanel = std::make_shared<OutlinerEditorPanel>();
    AddEditorPanel("OutlinerPanel", OutlinerPanel);

    auto PropertyPanel = std::make_shared<PropertyEditorPanel>();
    AddEditorPanel("PropertyPanel", PropertyPanel);

    auto SubWIndowPropertyPanel = std::make_shared<PropertyEditorPanel>();
    AddEditorPanel("PropertyPanel", SubWIndowPropertyPanel, true);

    auto SubWindowSkeletalTreePanel = std::make_shared<SkeletalTreePanel>();
    AddEditorPanel("SubWindowSkeletalTreePanel", SubWindowSkeletalTreePanel, true);
    
    // SkeletalMeshViewer용 컨트롤 패널 추가
    auto SkeletalControlPanel = std::make_shared<SkeletalMeshControlPanel>();
    AddEditorPanel("SkeletalControlPanel", SkeletalControlPanel, true);
}

void UnrealEd::Render() const
{
    for (const auto& Panel : Panels)
    {
        Panel.Value->Render();
    }
}

void UnrealEd::RenderSubWindowPanel() const
{
    for (const auto& Panel : SubPanels)
    {
        Panel.Value->Render();
    }
}

void UnrealEd::AddEditorPanel(const FString& PanelId, const std::shared_ptr<UEditorPanel>& EditorPanel, bool bSubWindow)
{
    (bSubWindow ? SubPanels : Panels)[PanelId] = EditorPanel;
}

void UnrealEd::OnResize(HWND hWnd, bool bSubWindow) const
{
    auto& targetPanels = bSubWindow ? SubPanels : Panels;

    for (auto& PanelPair : targetPanels)
    {
        if (PanelPair.Value)
        {
            PanelPair.Value->OnResize(hWnd);
        }
    }
}

std::shared_ptr<UEditorPanel> UnrealEd::GetEditorPanel(const FString& PanelId)
{
    return Panels[PanelId];
}

std::shared_ptr<UEditorPanel> UnrealEd::GetSubEditorPanel(const FString& PanelId)
{
    return SubPanels[PanelId];
}
