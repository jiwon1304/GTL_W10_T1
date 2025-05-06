#include "UnrealEd.h"
#include "EditorPanel.h"
#include "AssetViewer/AssetViewerWindow.h" // Include AssetViewerWindow.h
#include "Launch/EngineLoop.h" // For GEngineLoop

#include "PropertyEditor/ControlEditorPanel.h"
#include "PropertyEditor/OutlinerEditorPanel.h"
#include "PropertyEditor/PropertyEditorPanel.h"

extern FEngineLoop GEngineLoop; // Make sure GEngineLoop is accessible

UnrealEd::UnrealEd()
    : AssetViewerWindow(nullptr)
{
}

void UnrealEd::Initialize()
{
    auto ControlPanel = std::make_shared<ControlEditorPanel>();
    Panels["ControlPanel"] = ControlPanel;

    auto OutlinerPanel = std::make_shared<OutlinerEditorPanel>();
    Panels["OutlinerPanel"] = OutlinerPanel;

    auto PropertyPanel = std::make_shared<PropertyEditorPanel>();
    Panels["PropertyPanel"] = PropertyPanel;

    // AssetViewerWindow is created on demand by ToggleAssetViewerWindow
}

void UnrealEd::Render() const
{
    for (const auto& Panel : Panels)
    {
        Panel.Value->Render();
    }
}

void UnrealEd::AddEditorPanel(const FString& PanelId, const std::shared_ptr<UEditorPanel>& EditorPanel)
{
    Panels[PanelId] = EditorPanel;
}

void UnrealEd::OnResize(HWND hWnd) const
{
    for (auto& Panel : Panels)
    {
        Panel.Value->OnResize(hWnd);
    }
}

std::shared_ptr<UEditorPanel> UnrealEd::GetEditorPanel(const FString& PanelId)
{
    return Panels[PanelId];
}

void UnrealEd::ToggleAssetViewerWindow()
{
    if (!AssetViewerWindow)
    {
        AssetViewerWindow = std::make_shared<FAssetViewerWindow>();
    }

    // HINSTANCE is required for window creation.
    // In a typical UE application, this might come from FPlatformApplicationMisc::GetApplicationInstance()
    // or be stored if the main application window handle (HWND) is known.
    // For this project, assuming GEngineLoop.AppWnd is the main window HWND.
    HINSTANCE hInstance = nullptr;
    if (GEngineLoop.AppWnd)
    {
        hInstance = (HINSTANCE)GetWindowLongPtr(GEngineLoop.AppWnd, GWLP_HINSTANCE);
    }
    
    if (hInstance)
    {
        AssetViewerWindow->ToggleWindow(hInstance);
    }
    else
    {
        UE_LOG(ELogLevel::Error, TEXT("Failed to get HINSTANCE for AssetViewerWindow."));
    }
}
