#pragma once
#include "Core/HAL/PlatformType.h"

class UImGuiManager
{
public:
    void Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
    void BeginFrame() const;
    void EndFrame() const;
    static void PreferenceStyle();
    ImGuiContext* GetContext() const;
    void Shutdown();

    static void ApplySharedStyle(ImGuiContext* Context1, ImGuiContext* Context2);
    ImFont* SharedFont;

private:
    ImGuiContext* ImGuiContext = nullptr;
};

