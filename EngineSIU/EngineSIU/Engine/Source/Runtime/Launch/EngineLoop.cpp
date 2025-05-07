#include "EngineLoop.h"
#include "ImGuiManager.h"
#include "UnrealClient.h"
#include "WindowsPlatformTime.h"
#include "D3D11RHI/GraphicDevice.h"
#include "Engine/EditorEngine.h"
#include "LevelEditor/SLevelEditor.h"
#include "AssetViewer/AssetViewer.h"
#include "PropertyEditor/ViewportTypePanel.h"
#include "Slate/Widgets/Layout/SSplitter.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UnrealEd/UnrealEd.h"
#include "World/World.h"
#include "Renderer/TileLightCullingPass.h"
#include "SoundManager.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

FGraphicsDevice FEngineLoop::GraphicDevice;
FRenderer FEngineLoop::Renderer;
UPrimitiveDrawBatch FEngineLoop::PrimitiveDrawBatch;
FResourceMgr FEngineLoop::ResourceManager;
uint32 FEngineLoop::TotalAllocationBytes = 0;
uint32 FEngineLoop::TotalAllocationCount = 0;

FEngineLoop::FEngineLoop()
    : MainAppWnd(nullptr)
    , SkeletalMeshViewerAppWnd(nullptr)
    , MainUIManager(nullptr)
    , SkeletalMeshViewerUIManager(nullptr)
    , LevelEditor(nullptr)
    , AssetViewer(nullptr)
    , UnrealEditor(nullptr)
    , BufferManager(nullptr)
    , CurrentImGuiContext(nullptr)
{
}

int32 FEngineLoop::PreInit()
{
    return 0;
}

int32 FEngineLoop::Init(HINSTANCE hInstance)
{
    FPlatformTime::InitTiming();

    WindowInit(hInstance);
    SubWindowInit(hInstance);

    BufferManager = new FDXDBufferManager();
    MainUIManager = new UImGuiManager;
    SkeletalMeshViewerUIManager = new UImGuiManager;
    LevelEditor = new SLevelEditor();
    AssetViewer = new SAssetViewer();
    UnrealEditor = new UnrealEd();

    AppMessageHandler = std::make_unique<FSlateAppMessageHandler>();

    GraphicDevice.Initialize(MainAppWnd);
    GraphicDevice.CreateAdditionalSwapChain(SkeletalMeshViewerAppWnd);
    BufferManager->Initialize(GraphicDevice.Device, GraphicDevice.DeviceContext);
    Renderer.Initialize(&GraphicDevice, BufferManager, &GPUTimingManager);
    PrimitiveDrawBatch.Initialize(&GraphicDevice);
    ResourceManager.Initialize(&Renderer, &GraphicDevice);

    GEngine = FObjectFactory::ConstructObject<UEditorEngine>(nullptr);
    GEngine->Init();

    MainUIManager->Initialize(MainAppWnd, GraphicDevice.Device, GraphicDevice.DeviceContext);
    SkeletalMeshViewerUIManager->Initialize(SkeletalMeshViewerAppWnd, GraphicDevice.Device, GraphicDevice.DeviceContext);
    uint32 ClientWidth = 0;
    uint32 ClientHeight = 0;
    GetClientSize(MainAppWnd, ClientWidth, ClientHeight);
    LevelEditor->Initialize(ClientWidth, ClientHeight);
    GetClientSize(SkeletalMeshViewerAppWnd, ClientWidth, ClientHeight);
    AssetViewer->Initialize(ClientWidth, ClientHeight);
    UnrealEditor->Initialize();
    
    {
        if (!GPUTimingManager.Initialize(GraphicDevice.Device, GraphicDevice.DeviceContext))
        {
            UE_LOG(ELogLevel::Error, TEXT("Failed to initialize GPU Timing Manager!"));
        }
        EngineProfiler.SetGPUTimingManager(&GPUTimingManager);

        // @todo Table에 Tree 구조로 넣을 수 있도록 수정
        EngineProfiler.RegisterStatScope(TEXT("Renderer_Render"), FName(TEXT("Renderer_Render_CPU")), FName(TEXT("Renderer_Render_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- DepthPrePass"), FName(TEXT("DepthPrePass_CPU")), FName(TEXT("DepthPrePass_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- TileLightCulling"), FName(TEXT("TileLightCulling_CPU")), FName(TEXT("TileLightCulling_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- ShadowPass"), FName(TEXT("ShadowPass_CPU")), FName(TEXT("ShadowPass_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- StaticMeshPass"), FName(TEXT("StaticMeshPass_CPU")), FName(TEXT("StaticMeshPass_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- WorldBillboardPass"), FName(TEXT("WorldBillboardPass_CPU")), FName(TEXT("WorldBillboardPass_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- UpdateLightBufferPass"), FName(TEXT("UpdateLightBufferPass_CPU")), FName(TEXT("UpdateLightBufferPass_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- FogPass"), FName(TEXT("FogPass_CPU")), FName(TEXT("FogPass_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- PostProcessCompositing"), FName(TEXT("PostProcessCompositing_CPU")), FName(TEXT("PostProcessCompositing_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- EditorBillboardPass"), FName(TEXT("EditorBillboardPass_CPU")), FName(TEXT("EditorBillboardPass_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- EditorRenderPass"), FName(TEXT("EditorRenderPass_CPU")), FName(TEXT("EditorRenderPass_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- LinePass"), FName(TEXT("LinePass_CPU")), FName(TEXT("LinePass_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- GizmoPass"), FName(TEXT("GizmoPass_CPU")), FName(TEXT("GizmoPass_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("|- CompositingPass"), FName(TEXT("CompositingPass_CPU")), FName(TEXT("CompositingPass_GPU")));
        EngineProfiler.RegisterStatScope(TEXT("SlatePass"), FName(TEXT("SlatePass_CPU")), FName(TEXT("SlatePass_GPU")));
    }


    FSoundManager::GetInstance().Initialize();
    //FSoundManager::GetInstance().LoadSound("fishdream", "Contents/Sounds/fishdream.mp3");
    //FSoundManager::GetInstance().LoadSound("sizzle", "Contents/Sounds/sizzle.mp3");
    //FSoundManager::GetInstance().PlaySound("fishdream");

    // @todo 필요한 로직인지 확인
    UpdateUI();

    return 0;
}

void FEngineLoop::Render() const
{
    GraphicDevice.Prepare(MainAppWnd);
    
    if (LevelEditor->IsMultiViewport())
    {
        std::shared_ptr<FEditorViewportClient> ActiveViewportCache = GetLevelEditor()->GetActiveViewportClient();
        for (int i = 0; i < 4; ++i)
        {
            LevelEditor->SetActiveViewportClient(i);
            Renderer.Render(LevelEditor->GetActiveViewportClient());
        }
        
        for (int i = 0; i < 4; ++i)
        {
            LevelEditor->SetActiveViewportClient(i);
            Renderer.RenderViewport(MainAppWnd, LevelEditor->GetActiveViewportClient());
        }
        GetLevelEditor()->SetActiveViewportClient(ActiveViewportCache);
    }
    else
    {
        Renderer.Render(LevelEditor->GetActiveViewportClient());
        
        Renderer.RenderViewport(MainAppWnd, LevelEditor->GetActiveViewportClient());
    }

    GraphicDevice.Prepare(SkeletalMeshViewerAppWnd);

    if (SkeletalMeshViewerAppWnd && IsWindowVisible(SkeletalMeshViewerAppWnd) && AssetViewer)
    {
        // ActiveWorld를 변경하여 FRenderer::Render()에서 EditorPreviewWorld를 접근하도록 함
        UWorld* CurrentWorld = GEngine->ActiveWorld;
        if (UEditorEngine* E = Cast<UEditorEngine>(GEngine))
        {
            UWorld* EditorWorld = E->EditorPreviewWorld;
            if (EditorWorld)
            {
                GEngine->ActiveWorld = EditorWorld;
                Renderer.Render(AssetViewer->GetActiveViewportClient());
                auto Viewport = AssetViewer->GetActiveViewportClient();
                auto Location = Viewport->GetCameraLocation();

                UE_LOG(ELogLevel::Display, TEXT("%f %f %f"), Location.X, Location.Y, Location.Z);
            }
            Renderer.RenderViewport(SkeletalMeshViewerAppWnd, AssetViewer->GetActiveViewportClient());
        }

        GEngine->ActiveWorld = CurrentWorld;
    }
}

void FEngineLoop::WndMessageProc(HWND hWnd)
{
    MSG Msg;
    while (PeekMessage(&Msg, hWnd, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Msg); // 키보드 입력 메시지를 문자메시지로 변경
        DispatchMessage(&Msg);  // 메시지를 WndProc에 전달
        if (Msg.message == WM_QUIT)
        {
            bIsExit = true;
            break;
        }
    }
}

void FEngineLoop::Tick()
{
    LARGE_INTEGER Frequency;
    const double TargetFrameTime = 1000.0 / TargetFPS; // 한 프레임의 목표 시간 (밀리초 단위)

    QueryPerformanceFrequency(&Frequency);

    LARGE_INTEGER StartTime, EndTime;
    double ElapsedTime = 0.0;

    while (bIsExit == false)
    {
        QueryPerformanceCounter(&StartTime);

        FProfilerStatsManager::BeginFrame();    // Clear previous frame stats
        if (GPUTimingManager.IsInitialized())
        {
            GPUTimingManager.BeginFrame();      // Start GPU frame timing
        }

        /* Window Message Loop */
        WndMessageProc(MainAppWnd);
        if (SkeletalMeshViewerAppWnd && IsWindowVisible(SkeletalMeshViewerAppWnd) && !bIsExit)
        {
            WndMessageProc(SkeletalMeshViewerAppWnd);
        }
        if (bIsExit)
        {
            break;
        }

        /* Tick Game Logic */
        const float DeltaTime = static_cast<float>(ElapsedTime / 1000.f);
        GEngine->Tick(DeltaTime);
        LevelEditor->Tick(DeltaTime);
        AssetViewer->Tick(DeltaTime);
        // @todo SkeletalMeshViewer->Tick(DeltaTime);

        /* Render Viewports */
        Render();

        /* Render UI (ImGui) */
        // 메인 윈도우 ImGui
        if (MainUIManager && MainUIManager->GetContext() && IsWindowVisible(MainAppWnd))
        {
            MainUIManager->BeginFrame();
            {
                UnrealEditor->Render();

                FConsole::GetInstance().Draw();
                EngineProfiler.Render(GraphicDevice.DeviceContext, *GraphicDevice.ScreenWidths.Find(MainAppWnd), *GraphicDevice.ScreenHeights.Find(MainAppWnd));
                TempRenderDebugImGui();
                TempRenderDebugSubImGui();
            }
            ID3D11RenderTargetView* const* BackBufferRTV = Renderer.Graphics->BackBufferRTVs.Find(MainAppWnd);
            Renderer.Graphics->DeviceContext->OMSetRenderTargets(1, BackBufferRTV, nullptr);
            MainUIManager->EndFrame();
        }

        // 스켈레탈 메쉬 뷰어 ImGui
        if (SkeletalMeshViewerUIManager && SkeletalMeshViewerUIManager->GetContext() && SkeletalMeshViewerAppWnd && IsWindowVisible(SkeletalMeshViewerAppWnd))
        {
            SkeletalMeshViewerUIManager->BeginFrame();
            {
                UnrealEditor->RenderSubWindowPanel();
            }
            ID3D11RenderTargetView* const* BackBufferRTV = Renderer.Graphics->BackBufferRTVs.Find(SkeletalMeshViewerAppWnd);
            Renderer.Graphics->DeviceContext->OMSetRenderTargets(1, BackBufferRTV, nullptr);
            SkeletalMeshViewerUIManager->EndFrame();
        }

        // Pending 처리된 오브젝트 제거
        GUObjectArray.ProcessPendingDestroyObjects();

        if (GPUTimingManager.IsInitialized())
        {
            GPUTimingManager.EndFrame();        // End GPU frame timing
        }

        GraphicDevice.SwapBuffer(MainAppWnd);
        if (SkeletalMeshViewerAppWnd && IsWindowVisible(SkeletalMeshViewerAppWnd))
        {
            GraphicDevice.SwapBuffer(SkeletalMeshViewerAppWnd);
        }

        do
        {
            Sleep(0);
            QueryPerformanceCounter(&EndTime);
            ElapsedTime = (static_cast<double>(EndTime.QuadPart - StartTime.QuadPart) * 1000.f / static_cast<double>(Frequency.QuadPart));
        } while (ElapsedTime < TargetFrameTime);
    }
}

void FEngineLoop::GetClientSize(const HWND hWnd, uint32& OutWidth, uint32& OutHeight) const
{
    RECT ClientRect = {};
    GetClientRect(hWnd, &ClientRect);
            
    OutWidth = ClientRect.right - ClientRect.left;
    OutHeight = ClientRect.bottom - ClientRect.top;
}

void FEngineLoop::Exit()
{
    if (AssetViewer)
    {
        AssetViewer->Release();
        delete AssetViewer;
        AssetViewer = nullptr;
    }

    if (SkeletalMeshViewerAppWnd && IsWindow(SkeletalMeshViewerAppWnd))
    {
        DestroyWindow(SkeletalMeshViewerAppWnd);
        SkeletalMeshViewerAppWnd = nullptr;
    }
    if (SkeletalMeshViewerUIManager)
    {
        SkeletalMeshViewerUIManager->Shutdown();
        delete SkeletalMeshViewerUIManager;
        SkeletalMeshViewerUIManager = nullptr;
    }

    if (LevelEditor)
    {
        LevelEditor->Release();
        delete LevelEditor;
        LevelEditor = nullptr;
    }

    if (MainAppWnd && IsWindow(MainAppWnd))
    {
        DestroyWindow(MainAppWnd);
        MainAppWnd = nullptr;
    }
    if (MainUIManager)
    {
        MainUIManager->Shutdown();
        delete MainUIManager;
        MainUIManager = nullptr;
    }

    ResourceManager.Release(&Renderer);
    Renderer.Release();
    GraphicDevice.Release();
    GEngine->Release();

    delete UnrealEditor;
    delete BufferManager;
}

void FEngineLoop::WindowInit(HINSTANCE hInstance)
{
    WCHAR WindowClass[] = L"JungleWindowClass";

    WCHAR Title[] = L"Game Tech Lab";

    WNDCLASSW wc{};
    wc.lpfnWndProc = AppWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WindowClass;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

    RegisterClassW(&wc);

    MainAppWnd = CreateWindowExW(
        0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 1000,
        nullptr, nullptr, hInstance, nullptr
    );
}

void FEngineLoop::SubWindowInit(HINSTANCE hInstance)
{
    WCHAR WindowClass[] = L"JungleSubWindowClass";
    WCHAR Title[] = L"SkeletalMesh Viewer";

    WNDCLASSW wc{};
    wc.lpfnWndProc = SubAppWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WindowClass;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

    RegisterClassW(&wc);

    // WS_VISIBLE 제거, 숨김 초기 상태
    SkeletalMeshViewerAppWnd = CreateWindowExW(
        0, WindowClass, Title, WS_POPUP | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1800, 1100,
        MainAppWnd, nullptr, hInstance, nullptr
    );

    // @note 생성 확인용
    if (SkeletalMeshViewerAppWnd)
    {
        ShowWindow(SkeletalMeshViewerAppWnd, SW_SHOW);
        UpdateWindow(SkeletalMeshViewerAppWnd);
    }
}

LRESULT CALLBACK FEngineLoop::AppWndProc(HWND hWnd, uint32 Msg, WPARAM wParam, LPARAM lParam)
{
    if  (GEngineLoop.MainUIManager)
    {
        // 항상 메인 윈도우 컨텍스트로 설정하고 이벤트 처리
        ImGui::SetCurrentContext(GEngineLoop.MainUIManager->GetContext());
        GEngineLoop.CurrentImGuiContext = ImGui::GetCurrentContext();
        if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
        {
            return true;
        }
    }

    switch (Msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        if (auto LevelEditor = GEngineLoop.GetLevelEditor())
        {
            LevelEditor->SaveConfig();
        }
        if (auto AssetViewer = GEngineLoop.GetAssetViewer())
        {
            AssetViewer->SaveConfig();
        }
        /** Todo: 현재 PostQuitMessage의 종료 메시지가 정상적으로 수신되지 않아
         *  `bIsExit`을 강제로 true로 만들어주었습니다. 나중에 수정이 필요합니다.
         */
        GEngineLoop.bIsExit = true;
        break;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            if (auto LevelEditor = GEngineLoop.GetLevelEditor())
            {
                FEngineLoop::GraphicDevice.Resize(hWnd);
                // FEngineLoop::Renderer.DepthPrePass->ResizeDepthStencil();
                
                uint32 ClientWidth = 0;
                uint32 ClientHeight = 0;
                GEngineLoop.GetClientSize(hWnd, ClientWidth, ClientHeight);
            
                LevelEditor->ResizeEditor(ClientWidth, ClientHeight);
                FEngineLoop::Renderer.TileLightCullingPass->ResizeViewBuffers(
                    static_cast<uint32>(LevelEditor->GetActiveViewportClient()->GetD3DViewport().Width),
                    static_cast<uint32>(LevelEditor->GetActiveViewportClient()->GetD3DViewport().Height)
                );
            }
        }
        GEngineLoop.UpdateUI();
        break;
    case WM_ACTIVATE:
        if (ImGui::GetCurrentContext() == nullptr || !GEngineLoop.MainUIManager)
        {
            break;
        }
        
        // 윈도우가 비활성화될 때 키 상태 초기화
        if (wParam == WA_INACTIVE)
        {
            if (GEngineLoop.GetLevelEditor())
            {
                auto* LevelEditor = GEngineLoop.GetLevelEditor();
                for (int i = 0; i < 4; ++i)  // ViewportClients는 고정 크기 4개의 배열
                {
                    auto ViewportClient = LevelEditor->GetViewports()[i];
                    if (ViewportClient)
                    {
                        ViewportClient->ResetKeyState();
                    }
                }
            }
        }
        // 활성화 상태일 때만 컨텍스트 설정 (WA_ACTIVE=1, WA_CLICKACTIVE=2)
        else if (wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE)
        {
            ImGui::SetCurrentContext(GEngineLoop.MainUIManager->GetContext());
            GEngineLoop.CurrentImGuiContext = ImGui::GetCurrentContext();
        }
        break;
    default:
        GEngineLoop.AppMessageHandler->ProcessMessage(hWnd, Msg, wParam, lParam);
    }

    return DefWindowProc(hWnd, Msg, wParam, lParam);
}

LRESULT FEngineLoop::SubAppWndProc(HWND hWnd, uint32 Msg, WPARAM wParam, LPARAM lParam)
{
    if (GEngineLoop.SkeletalMeshViewerUIManager)
    {
        // 항상 서브 윈도우 컨텍스트로 설정하고 이벤트 처리
        ImGui::SetCurrentContext(GEngineLoop.SkeletalMeshViewerUIManager->GetContext());
        GEngineLoop.CurrentImGuiContext = ImGui::GetCurrentContext();
        if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
        {
            return true;
        }
    }

    switch (Msg)
    {
    case WM_DESTROY:
        // Do Nothing (ShowWindow로 관리하므로 직접 처리할 필요 없음. DESTROY가 호출될 때에는 Exit에서 일괄처리함)
        break;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            // 클라이언트 영역 크기 가져오기
            uint32 ClientWidth = 0;
            uint32 ClientHeight = 0;
            GEngineLoop.GetClientSize(hWnd, ClientWidth, ClientHeight);
            
            // AssetViewer 리사이즈
            if (auto AssetViewer = GEngineLoop.GetAssetViewer())
            {
                AssetViewer->ResizeEditor(ClientWidth, ClientHeight);
                if (AssetViewer->GetActiveViewportClient())
                {
                    // 필요하다면 TileLightCullingPass 버퍼도 리사이즈
                    FEngineLoop::Renderer.TileLightCullingPass->ResizeViewBuffers(
                        static_cast<uint32>(AssetViewer->GetActiveViewportClient()->GetD3DViewport().Width),
                        static_cast<uint32>(AssetViewer->GetActiveViewportClient()->GetD3DViewport().Height)
                    );
                }
            }
        }
        // UI 업데이트는 기존대로 호출
        GEngineLoop.UpdateSubWindowUI();
        break;
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;
    case WM_ACTIVATE:
        if (ImGui::GetCurrentContext() == nullptr || !GEngineLoop.SkeletalMeshViewerUIManager)
        {
            break;
        }
        
        // 윈도우가 비활성화될 때 키 상태 초기화
        if (wParam == WA_INACTIVE)
        {
            if (GEngineLoop.GetAssetViewer() && GEngineLoop.GetAssetViewer()->GetActiveViewportClient())
            {
                GEngineLoop.GetAssetViewer()->GetActiveViewportClient()->ResetKeyState();
            }
        }
        // 활성화 상태일 때만 컨텍스트 설정 (WA_ACTIVE=1, WA_CLICKACTIVE=2)
        else if (wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE)
        {
            ImGui::SetCurrentContext(GEngineLoop.SkeletalMeshViewerUIManager->GetContext());
            GEngineLoop.CurrentImGuiContext = ImGui::GetCurrentContext();
        }
        break;
    default:
        // @todo MessageHandler 수정
        GEngineLoop.AppMessageHandler->ProcessMessage(hWnd, Msg, wParam, lParam);
        break;
    }

    return DefWindowProc(hWnd, Msg, wParam, lParam);
}

void FEngineLoop::UpdateUI()
{
    FConsole::GetInstance().OnResize(MainAppWnd);
    if (GEngineLoop.GetUnrealEditor())
    {
        GEngineLoop.GetUnrealEditor()->OnResize(MainAppWnd);
    }
    ViewportTypePanel::GetInstance().OnResize(MainAppWnd);
}

void FEngineLoop::UpdateSubWindowUI()
{
    if (GEngineLoop.GetUnrealEditor())
    {
        GEngineLoop.GetUnrealEditor()->OnResize(SkeletalMeshViewerAppWnd, true);
    }
    
    ViewportTypePanel::GetInstance().OnResize(SkeletalMeshViewerAppWnd);
}

void FEngineLoop::ToggleWindow(const HWND hWnd)
{
    if (hWnd && !IsWindowVisible(hWnd))
    {
        ShowWindow(hWnd, SW_SHOWDEFAULT);
        UpdateWindow(hWnd);
    }
    else
    {
        ShowWindow(hWnd, SW_HIDE);
    }
}

void FEngineLoop::Show(const HWND HWnd)
{
    if (HWnd)
    {
        ImGuiContext* PreviousContext = ImGui::GetCurrentContext();

        ShowWindow(HWnd, SW_SHOWDEFAULT);
        UpdateWindow(HWnd);

        // 기존 ImGuiContext 복원 (다른 컨텍스트에서 호출되어도 문제가 없게끔)
        if (PreviousContext)
        {
            ImGui::SetCurrentContext(PreviousContext);
        }
    }
}

void FEngineLoop::Hide(const HWND hWnd)
{
    if (hWnd)
    {
        ShowWindow(hWnd, SW_HIDE);
    }
}

void FEngineLoop::TempRenderDebugImGui()
{
    // ImGui 디버그 정보 표시
    ImGui::Begin("Splitter Debug Info");

    // 마우스 커서 위치 가져오기
    POINT MousePos;
    GetCursorPos(&MousePos);
    ScreenToClient(GEngineLoop.MainAppWnd, &MousePos);
    FVector2D ClientPos = FVector2D{ static_cast<float>(MousePos.x), static_cast<float>(MousePos.y) };

    ImGui::Text("Mouse Position: (%.1ld, %.1ld)", MousePos.x, MousePos.y);
    ImGui::Text("Client Position: (%.1f, %.1f)", ClientPos.X, ClientPos.Y);

    FRect Rect = LevelEditor->GetActiveViewportClient()->GetViewport()->GetRect();

    // 현재 Splitter의 Rect 정보 표시
    ImGui::Text("Selected Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);

    ImGui::SeparatorText("Splitters");
    Rect = LevelEditor->MainVSplitter->GetRect();
    ImGui::Text("MainSplitter Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = LevelEditor->MainVSplitter->SideLT->GetRect();
    ImGui::Text("- MainSplitter SideLT Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = LevelEditor->MainVSplitter->SideRB->GetRect();
    ImGui::Text("- MainSplitter SideRB Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = LevelEditor->EditorHSplitter->GetRect();
    ImGui::Text("EditorSplitter Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = LevelEditor->EditorHSplitter->SideLT->GetRect();
    ImGui::Text("- EditorSplitter SideLT Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = LevelEditor->EditorHSplitter->SideRB->GetRect();
    ImGui::Text("- EditorSplitter SideRB Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = LevelEditor->ViewportVSplitter->GetRect();
    ImGui::Text("ViewportVSplitter Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = LevelEditor->ViewportHSplitter->GetRect();
    ImGui::Text("ViewportHSplitter Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);

    ImGui::SeparatorText("Viewports");
    for (int i = 0; i < 4; ++i)
    {
        auto ViewportClient = LevelEditor->GetViewports()[i];
        if (ViewportClient)
        {
            auto Viewport = ViewportClient->GetViewport();
            bool bIsSelected = Viewport == LevelEditor->GetActiveViewportClient()->GetViewport();
            ImGui::Text("Viewport Rect[%s]: Pos(%.1f, %.1f) Size(%.1f, %.1f)", bIsSelected ? "O" : "X", Viewport->GetRect().TopLeftX, Viewport->GetRect().TopLeftY, Viewport->GetRect().TopLeftX + Viewport->GetRect().Width, Viewport->GetRect().TopLeftY + Viewport->GetRect().Height);
        }
    }
    // IsSplitterHovered 상태 표시
    // IsSplitterHovered 함수는 const가 아니므로 const_cast 사용 또는 함수를 const로 변경 필요.
    // 여기서는 const_cast를 사용합니다.
    // FPoint 생성자에 float 대신 int32를 사용하도록 수정
    bool bIsMainHovered = LevelEditor->MainVSplitter->IsSplitterHovered(FPoint(static_cast<int32>(MousePos.x), static_cast<int32>(MousePos.y)));
    bool bIsEditorHovered = LevelEditor->EditorHSplitter->IsSplitterHovered(FPoint(static_cast<int32>(MousePos.x), static_cast<int32>(MousePos.y)));
    bool bIsViewportVHovered = LevelEditor->ViewportVSplitter->IsSplitterHovered(FPoint(static_cast<int32>(MousePos.x), static_cast<int32>(MousePos.y)));
    bool bIsViewportHHovered = LevelEditor->ViewportHSplitter->IsSplitterHovered(FPoint(static_cast<int32>(MousePos.x), static_cast<int32>(MousePos.y)));

    ImGui::SeparatorText("Hovered");
    ImGui::Text("IsSplitterHovered: %s", bIsMainHovered ? "true" : "false");
    ImGui::Text("IsEditorHovered: %s", bIsEditorHovered ? "true" : "false");
    ImGui::Text("IsViewportVHovered: %s", bIsViewportVHovered ? "true" : "false");
    ImGui::Text("IsViewportHHovered: %s", bIsViewportHHovered ? "true" : "false");

    ImGui::End();
}

void FEngineLoop::TempRenderDebugSubImGui()
{
    // ImGui 디버그 정보 표시
    ImGui::Begin("Splitter Debug Sub Info");

    // 마우스 커서 위치 가져오기
    POINT MousePos;
    GetCursorPos(&MousePos);
    ImGui::Text("Mouse Position: (%.1ld, %.1ld)", MousePos.x, MousePos.y);

    ScreenToClient(GEngineLoop.SkeletalMeshViewerAppWnd, &MousePos);
    FVector2D ClientPos = FVector2D{ static_cast<float>(MousePos.x), static_cast<float>(MousePos.y) };
    ImGui::Text("Client Position: (%.1f, %.1f)", ClientPos.X, ClientPos.Y);

    FRect Rect = AssetViewer->GetActiveViewportClient()->GetViewport()->GetRect();

    // 현재 Splitter의 Rect 정보 표시
    ImGui::Text("Selected Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);

    ImGui::SeparatorText("Splitters");
    Rect = AssetViewer->PrimaryVSplitter->GetRect();
    ImGui::Text("PrimaryVSplitter Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = AssetViewer->PrimaryVSplitter->SideLT->GetRect();
    ImGui::Text("- PrimaryVSplitter SideLT Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = AssetViewer->PrimaryVSplitter->SideRB->GetRect();
    ImGui::Text("- PrimaryVSplitter SideRB Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = AssetViewer->CenterAndRightVSplitter->GetRect();
    ImGui::Text("CenterAndRightVSplitter Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = AssetViewer->CenterAndRightVSplitter->SideLT->GetRect();
    ImGui::Text("- CenterAndRightVSplitter SideLT Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = AssetViewer->CenterAndRightVSplitter->SideRB->GetRect();
    ImGui::Text("- CenterAndRightVSplitter SideRB Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);
    Rect = AssetViewer->RightSidebarHSplitter->GetRect();
    ImGui::Text("RightSidebarHSplitter Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Rect.TopLeftX, Rect.TopLeftY, Rect.TopLeftX + Rect.Width, Rect.TopLeftY + Rect.Height);

    ImGui::SeparatorText("Viewport");
    auto ViewportClient = AssetViewer->GetActiveViewportClient();
    auto Viewport = ViewportClient->GetViewport();
    ImGui::Text("Viewport Rect: Pos(%.1f, %.1f) Size(%.1f, %.1f)", Viewport->GetRect().TopLeftX, Viewport->GetRect().TopLeftY, Viewport->GetRect().TopLeftX + Viewport->GetRect().Width, Viewport->GetRect().TopLeftY + Viewport->GetRect().Height);
    // IsSplitterHovered 상태 표시
    // IsSplitterHovered 함수는 const가 아니므로 const_cast 사용 또는 함수를 const로 변경 필요.
    // 여기서는 const_cast를 사용합니다.
    // FPoint 생성자에 float 대신 int32를 사용하도록 수정
    bool bIsPrimaryHovered = AssetViewer->PrimaryVSplitter->IsSplitterHovered(FPoint(static_cast<int32>(MousePos.x), static_cast<int32>(MousePos.y)));
    bool bIsCentralRightHovered = AssetViewer->CenterAndRightVSplitter->IsSplitterHovered(FPoint(static_cast<int32>(MousePos.x), static_cast<int32>(MousePos.y)));
    bool bIsRIghtSidebarHovered = AssetViewer->RightSidebarHSplitter->IsSplitterHovered(FPoint(static_cast<int32>(MousePos.x), static_cast<int32>(MousePos.y)));

    ImGui::SeparatorText("Hovered");
    ImGui::Text("IsPrimaryHovered: %s", bIsPrimaryHovered ? "true" : "false");
    ImGui::Text("IsCentralRightHovered: %s", bIsCentralRightHovered ? "true" : "false");
    ImGui::Text("IsRightSidebarHovered: %s", bIsRIghtSidebarHovered ? "true" : "false");

    ImGui::End();
}
