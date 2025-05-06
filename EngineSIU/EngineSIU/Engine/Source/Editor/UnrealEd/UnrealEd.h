#pragma once
#include "Container/Map.h"
#include "Container/String.h"

class UEditorPanel;
class FAssetViewerWindow; // Forward declaration

class UnrealEd
{
public:
    UnrealEd(); // Modified for AssetViewerWindow initialization
    ~UnrealEd() = default;
    void Initialize();
    
     void Render() const;
     void OnResize(HWND hWnd) const;
    
    void AddEditorPanel(const FString& PanelId, const std::shared_ptr<UEditorPanel>& EditorPanel);
    std::shared_ptr<UEditorPanel> GetEditorPanel(const FString& PanelId);

    void ToggleAssetViewerWindow();

private:
    TMap<FString, std::shared_ptr<UEditorPanel>> Panels;
    std::shared_ptr<FAssetViewerWindow> AssetViewerWindow;
};
