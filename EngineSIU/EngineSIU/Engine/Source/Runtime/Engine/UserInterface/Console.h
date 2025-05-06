#pragma once
#include "Container/Array.h"
#include "D3D11RHI/GraphicDevice.h"
#include "HAL/PlatformType.h"
#include "UObject/NameTypes.h"
#include "ImGui/imgui.h"
#include "PropertyEditor/IWindowToggleable.h"

static consteval const char* GetFileName(const char* Path)
{
    const char* Name = Path;
    while (*Path)
    {
        if (*Path == '/' || *Path == '\\')
        {
            Name = Path + 1;
        }
        Path++;
    }
    return Name;
}

#define FILENAME GetFileName(__FILE__)
#define UE_LOG(Level, Fmt, ...) FConsole::GetInstance().AddLog(Level, "[%s:%d] " Fmt, FILENAME, __LINE__, __VA_ARGS__)

enum class ELogLevel : uint8
{
    Display,
    Warning,
    Error
};

class FStatOverlay
{
public:
    // @todo Stat-FPS Default 설정 복구 (showFPS = false, showRender = false)
    // TODO: 추후에 enum 비트연산으로 바꿔도 좋을듯
    union
    {
        struct  // NOLINT(clang-diagnostic-nested-anon-types)
        {
            uint8 bShowFps : 1;
            uint8 bShowMemory : 1;
            uint8 bShowLight : 1;
            uint8 bShowRender : 1;
        };
        uint8 StatFlags = 0; // 기본적으로 다 끄기
    };

    void ToggleStat(const std::string& Command);
    void Render(ID3D11DeviceContext* Context, UINT Width, UINT Height) const;
};

struct FProfiledScope
{
    FString DisplayName;
    FName CPUStatName;
    FName GPUStatName;
};

class FGPUTimingManager;

class FEngineProfiler
{
public:
    FEngineProfiler() = default;
    ~FEngineProfiler() = default;

    void SetGPUTimingManager(FGPUTimingManager* InGPUTimingManager);
    void Render(ID3D11DeviceContext* Context, UINT Width, UINT Height);
    void RegisterStatScope(const FString& DisplayName, const FName& CPUStatName, const FName& GPUStatName);

    void ToggleWindow() { bShowWindow = !bShowWindow; }
    void ShowWindow() { bShowWindow = true; }
    void HideWindow() { bShowWindow = false; }

private:
    FGPUTimingManager* GPUTimingManager = nullptr;
    TArray<FProfiledScope> TrackedScopes;
    bool bShowWindow = true;
};

class FConsole : public IWindowToggleable
{
private:
    FConsole() = default;
    virtual ~FConsole() override = default;

public:
    // 복사 방지
    FConsole(const FConsole&) = delete;
    FConsole& operator=(const FConsole&) = delete;
    FConsole(FConsole&&) = delete;
    FConsole& operator=(FConsole&&) = delete;

public:
    static FConsole& GetInstance(); // 참조 반환으로 변경

    void Clear();
    void AddLog(ELogLevel Level, const ANSICHAR* Fmt, ...);
    void AddLog(ELogLevel Level, const WIDECHAR* Fmt, ...);
    void Draw();
    void ExecuteCommand(const std::string& Command);
    void OnResize(HWND hWnd);

    virtual void Toggle() override { bWasOpen = !bWasOpen; }

public:
    struct LogEntry
    {
        ELogLevel Level;
        FString Message;
    };

    TArray<LogEntry> Items;
    TArray<FString> History;
    int32 HistoryPos = -1;
    char InputBuf[256] = "";
    bool ScrollToBottom = false;

    ImGuiTextFilter Filter; // 필터링을 위한 ImGuiTextFilter

    bool ShowLogTemp = true; // LogTemp 체크박스
    bool ShowWarning = true; // Warning 체크박스
    bool ShowError = true;   // Error 체크박스

    bool bWasOpen = true;

    FStatOverlay Overlay;

private:
    bool bExpand = true;
    UINT Width;
    UINT Height;
};
