#pragma once
#include <sstream>
#include "Define.h"
#include "Container/Map.h"

// Forward declarations
class SSplitterV;
class SSplitterH;
class UWorld; // May not be needed directly, but SLevelEditor has it
class FEditorViewportClient;
class FDelegateHandle; // For input delegates

class SlateViewer
{
public:
    SlateViewer();

    void Initialize(HWND hWnd, const FString& ConfigPath, uint32 InEditorWidth, uint32 InEditorHeight);
    void Tick(float DeltaTime);
    void Release();

    void ResizeEditor(uint32 InEditorWidth, uint32 InEditorHeight);
    void SelectViewport(const FVector2D& Point) const;

    // 단일 뷰포트로 가정
    void ResizeViewport() const;

    void RegisterViewerInputDelegates();

    std::shared_ptr<FEditorViewportClient> GetActiveViewportClient() const { return ActiveViewportClient; }

    void LoadConfig();
    void SaveConfig();

    // Splitters for layout (중첩 구조)
    /*
     * PrimaryVSplitter (전체 창을 좌/우로 분할)
     *   ├─ SideLT (Left): 본 트리 패널 영역
     *   └─ SideRB (Right): CenterAndRightVSplitter (중앙 뷰포트 + 우측 사이드바 전체)
     *
     * CenterAndRightVSplitter (PrimaryVSplitter의 우측 영역을 다시 좌/우로 분할)
     *   ├─ SideLT (Left): 모델 렌더 뷰포트 (ActiveViewportClient) 영역
     *   └─ SideRB (Right): RightSidebarHSplitter (우측 사이드바 전체)
     *
     * RightSidebarHSplitter (우측 사이드바 영역을 상/하로 분할)
     *   ├─ SideLT (Top): 디테일 패널 영역
     *   └─ SideRB (Bottom): Temp 패널 영역
     */
    SSplitterV* PrimaryVSplitter;
    SSplitterV* CenterAndRightVSplitter;
    SSplitterH* RightSidebarHSplitter;

private:
    // Viewport Client for the main model rendering area
    std::shared_ptr<FEditorViewportClient> ActiveViewportClient;

    // Window Handle
    HWND Handle;
    
    // Input State
    /** 우클릭 시 캡처된 마우스 커서의 초기 위치 (스크린 좌표계) */
    FVector2D MousePinPosition;
    /** 좌클릭시 커서와 선택된 Asset(ex. Bone)과의 거리 차 */
    FVector TargetDiff;

    // Editor State
    uint32 EditorWidth;
    uint32 EditorHeight;

    // Delegate Handles
    TArray<FDelegateHandle> InputDelegatesHandles;

    // Configuration
    FString IniFilePath;

    // Helper functions
    // void CalculateViewportRects(...); // Replaced by direct FRect access from splitters
    TMap<FString, FString> ReadIniFile(const FString& FilePath);
    void WriteIniFile(const FString& FilePath, const TMap<FString, FString>& Config);

    template <typename T>
    T GetValueFromConfig(const TMap<FString, FString>& Config, const FString& Key, T DefaultValue) {
        if (const FString* Value = Config.Find(Key))
        {
            // For FString, no conversion needed
            if constexpr (std::is_same_v<T, FString>)
            {
                return *Value;
            }
            else
            {
                std::istringstream iss(**Value);
                T ConfigValue;
                if (iss >> ConfigValue) // This works for basic types like int, float, bool
                {
                    return ConfigValue;
                }
            }
        }
        return DefaultValue;
    }
};
