#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGuiManager.h"
#include "Font/RawFonts.h"
#include "Font/IconDefs.h"

void UImGuiManager::Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
    IMGUI_CHECKVERSION();
    ImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(ImGuiContext);

    ImGuiIO& io = ImGui::GetIO();
    // Win32 백엔드 초기화 (핸들 마다 초기화)
    ImGui_ImplWin32_Init(hWnd);
    // DX11 백엔드 초기화 (전역에 한 번만)
    if (io.BackendRendererUserData == nullptr)
    {
        ImGui_ImplDX11_Init(device, deviceContext);
    }

    io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\malgun.ttf)", 18.0f, nullptr, io.Fonts->GetGlyphRangesKorean());

    ImFontConfig FeatherFontConfig;
    FeatherFontConfig.PixelSnapH = true;
    FeatherFontConfig.FontDataOwnedByAtlas = false;
    FeatherFontConfig.GlyphOffset = ImVec2(0, 0);
    static constexpr ImWchar IconRanges[] = {
        ICON_MOVE,      ICON_MOVE + 1,
        ICON_ROTATE,    ICON_ROTATE + 1,
        ICON_SCALE,     ICON_SCALE + 1,
        ICON_MONITOR,   ICON_MONITOR + 1,
        ICON_BAR_GRAPH, ICON_BAR_GRAPH + 1,
        ICON_NEW,       ICON_NEW + 1,
        ICON_SAVE,      ICON_SAVE + 1,
        ICON_LOAD,      ICON_LOAD + 1,
        ICON_MENU,      ICON_MENU + 1,
        ICON_SLIDER,    ICON_SLIDER + 1,
        ICON_PLUS,      ICON_PLUS + 1,
        ICON_PLAY,      ICON_PLAY + 1,
        ICON_PAUSE,     ICON_PAUSE + 1,
        ICON_STOP,      ICON_STOP + 1,
        ICON_SQUARE,    ICON_SQUARE + 1,
        ICON_FORWARD,   ICON_FORWARD + 1,
        ICON_REWIND,    ICON_REWIND + 1,
        ICON_REPEAT,    ICON_REPEAT + 1,
        0 };

    io.Fonts->AddFontFromMemoryTTF(FeatherRawData, FontSizeOfFeather, 22.0f, &FeatherFontConfig, IconRanges);
    PreferenceStyle();
}

void UImGuiManager::BeginFrame() const
{
    ImGui::SetCurrentContext(ImGuiContext);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void UImGuiManager::EndFrame() const
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

/* GUI Style Preference */
void UImGuiManager::PreferenceStyle()
{
    ImGuiStyle& Style = ImGui::GetStyle();

    /** Colors */
    ImVec4 AccentColor = ImVec4(0.0f, 0.3f, 0.0f, 1.0f);
    ImVec4 PrimaryTextColor = ImGui::ColorConvertU32ToFloat4(IM_COL32(33, 33, 33, 255));

    Style.WindowRounding = 5.0f;
    Style.FrameRounding = 5.0f;

    /** Text accent color */
    Style.Colors[ImGuiCol_Text] = PrimaryTextColor;

    // Window
    Style.Colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.02f, 0.02f, 1);
    Style.Colors[ImGuiCol_Border] = ImVec4(0.098f, 0.098f, 0.098f, 1);

    Style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0f, 0.0f, 0.0f, 1);

    // Menubar
    Style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.0f);

    // Title
    Style.Colors[ImGuiCol_TitleBg] = ImVec4{ 0.05f, 0.05f, 0.05f, 1.0f };
    Style.Colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.1f, 0.1f, 0.1f, 1.0f };
    Style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.05f, 0.05f, 0.05f, 1.0f };

    // Header
    Style.Colors[ImGuiCol_Header] = ImVec4(1, 1, 1, 0.0f);
    Style.Colors[ImGuiCol_HeaderActive] = ImVec4(1, 1, 1, 0.3f);
    Style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1, 1, 1, 0.25f);

    // Button
    Style.Colors[ImGuiCol_Button] = ImVec4(0, 0, 0, 1.0f);
    Style.Colors[ImGuiCol_ButtonActive] = ImVec4(1, 1, 1, 0.3f);
    Style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1, 1, 1, 0.25f);

    // Frame
    Style.Colors[ImGuiCol_FrameBg] = ImVec4{ 0.039f, 0.039f, 0.039f, 1.0f };
    Style.Colors[ImGuiCol_FrameBgHovered] = ImVec4{ 1, 1, 1, 0.25f };
    Style.Colors[ImGuiCol_FrameBgActive] = ImVec4{ 1, 1, 1, 0.3f };

    // Check
    Style.Colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.30f, 0.00f, 1.0f);

    // Slider
    Style.Colors[ImGuiCol_SliderGrab] = AccentColor;
    Style.Colors[ImGuiCol_SliderGrabActive] = AccentColor;

    // ResizeGrip
    Style.Colors[ImGuiCol_ResizeGrip] = AccentColor;
    Style.Colors[ImGuiCol_ResizeGripHovered] = AccentColor;
    Style.Colors[ImGuiCol_ResizeGripActive] = AccentColor;

    // Tabs
    ImGui::GetStyle().Colors[ImGuiCol_Tab] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.15f };
    ImGui::GetStyle().Colors[ImGuiCol_TabHovered] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.15f };
    ImGui::GetStyle().Colors[ImGuiCol_TabActive] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.15f };
    ImGui::GetStyle().Colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    ImGui::GetStyle().Colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.15f };
}

ImGuiContext* UImGuiManager::GetContext() const
{
    return ImGuiContext;
}

void UImGuiManager::Shutdown()
{
    ImGui::SetCurrentContext(ImGuiContext);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(ImGuiContext);
}

void UImGuiManager::ApplySharedStyle(::ImGuiContext* Context1, ::ImGuiContext* Context2)
{
    ImGui::SetCurrentContext(Context1);
    ImGuiStyle& Style = ImGui::GetStyle();
    ImFont* Font = ImGui::GetIO().FontDefault;

    ImGui::SetCurrentContext(Context2);
    ImGui::GetStyle() = Style;
    ImGui::GetIO().FontDefault = Font;
}
