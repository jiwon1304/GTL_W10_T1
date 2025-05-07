#pragma once
#include <sstream>
#include "Define.h"
#include "Container/Map.h"

class SSplitterH;
class SSplitterV;
class UWorld;
class FEditorViewportClient;
class FDelegateHandle;

class SLevelEditor
{
public:
    SLevelEditor();

    void Initialize(uint32 InEditorWidth, uint32 InEditorHeight);
    void Tick(float DeltaTime);
    void Release() const;

    void ResizeEditor(uint32 InEditorWidth, uint32 InEditorHeight);
    void SelectViewport(const FVector2D& Point);

    void ResizeViewports();
    void SetEnableMultiViewport(bool bIsEnable);
    bool IsMultiViewport() const;

    void RegisterEditorInputDelegates();
    void RegisterPIEInputDelegates();

    // SSplitterH* HSplitter; // Replaced
    // SSplitterV* VSplitter; // Replaced
    /*
     *    + ------- + -------- + ---- +
     *    |         |          |      |
     *    |         |          + ---- +
     *    + ------- + -------- +      |
     *    |         |          |      |
     *    |         |          |      |
     *    + ------- + -------- + ---- +
     */
    SSplitterV* MainVSplitter;
    SSplitterH* EditorHSplitter;
    SSplitterV* ViewportVSplitter;
    SSplitterH* ViewportHSplitter;
private:

    std::shared_ptr<FEditorViewportClient> ViewportClients[4];
    std::shared_ptr<FEditorViewportClient> ActiveViewportClient;

    /** 우클릭 시 캡처된 마우스 커서의 초기 위치 (스크린 좌표계) */
    FVector2D MousePinPosition;

    /** 좌클릭시 커서와 선택된 Actor와의 거리 차 */
    FVector TargetDiff;

    bool bMultiViewportMode;
    
    uint32 EditorWidth;
    uint32 EditorHeight;

    TArray<FDelegateHandle> InputDelegatesHandles;
public:
    std::shared_ptr<FEditorViewportClient>* GetViewports() { return ViewportClients; }
    std::shared_ptr<FEditorViewportClient> GetActiveViewportClient() const
    {
        return ActiveViewportClient;
    }
    void SetActiveViewportClient(const std::shared_ptr<FEditorViewportClient>& InViewportClient)
    {
        ActiveViewportClient = InViewportClient;
    }
    void SetActiveViewportClient(int Index)
    {
        ActiveViewportClient = ViewportClients[Index];
    }

    // Get the designated area for ImGui panels
    FRect GetPanelAreaRect() const;

    //Save And Load
private:
    const FString IniFilePath = "editor.ini";

public:
    void LoadConfig();
    void SaveConfig();

private:
    // Helper function to calculate viewport rects based on splitters
    void CalculateViewportRects(const FRect& ViewportArea, FRect& OutTopLeft, FRect& OutTopRight, FRect& OutBottomLeft, FRect& OutBottomRight) const;

    TMap<FString, FString> ReadIniFile(const FString& FilePath);
    void WriteIniFile(const FString& FilePath, const TMap<FString, FString>& Config);

    template <typename T>
    T GetValueFromConfig(const TMap<FString, FString>& Config, const FString& Key, T DefaultValue) {
        if (const FString* Value = Config.Find(Key))
        {
            std::istringstream iss(**Value);
            T ConfigValue;
            if (iss >> ConfigValue)
            {
                return ConfigValue;
            }
        }
        return DefaultValue;
    }
};
