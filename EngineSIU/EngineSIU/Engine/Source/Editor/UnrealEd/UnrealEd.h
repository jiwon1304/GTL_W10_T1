#pragma once
#include "Container/Map.h"
#include "Container/String.h"

enum EWindowType : uint8;
class UEditorPanel;

class UnrealEd
{
public:
    UnrealEd() = default;
    ~UnrealEd() = default;
    void Initialize();

    void Render() const;
    void RenderSubWindowPanel(HWND hWnd) const;
    void OnResize(HWND hWnd, bool bSubWindow = false) const;

    void AddEditorPanel(const FString& PanelId, const std::shared_ptr<UEditorPanel>& EditorPanel, bool bSubWindow = false);
    std::shared_ptr<UEditorPanel> GetEditorPanel(const FString& PanelId);
    std::shared_ptr<UEditorPanel> GetSubEditorPanel(const FString& PanelId);

private:
    TMap<FString, std::shared_ptr<UEditorPanel>> Panels;
    TMap<FString, std::shared_ptr<UEditorPanel>> SubPanels;
};
