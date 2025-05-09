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
    , AnimationViewerAppWnd(nullptr)
    , MainUIManager(nullptr)
    , SkeletalMeshViewerUIManager(nullptr)
    , AnimationViewerUIManager(nullptr)
    , CurrentImGuiContext(nullptr)
    , LevelEditor(nullptr)
    , AssetViewer(nullptr)
    , UnrealEditor(nullptr)
    , BufferManager(nullptr)
{
}

int32 FEngineLoop::PreInit()
{
    return 0;
}

int32 FEngineLoop::Init(HINSTANCE hInstance)
{
    FPlatformTime::InitTiming();

    /** Create Window */
    MainAppWnd = CreateNewWindow(hInstance, L"MainWindowClass", L"SIU Engine", 1400, 1000, nullptr);
    if (MainAppWnd)
    {
        ShowWindow(MainAppWnd, SW_SHOW);
    }
    SkeletalMeshViewerAppWnd = CreateNewWindow(hInstance, L"SkeletalWindowClass", L"SkeletalMesh Viewer", 800, 600, nullptr);
    
    /** New Constructor */
    BufferManager = new FDXDBufferManager();
    MainUIManager = new UImGuiManager;
    SkeletalMeshViewerUIManager = new UImGuiManager;
    LevelEditor = new SLevelEditor();
    AssetViewer = new SAssetViewer();
    UnrealEditor = new UnrealEd();
    AppMessageHandler = std::make_unique<FSlateAppMessageHandler>();
    GEngine = FObjectFactory::ConstructObject<UEditorEngine>(nullptr);

    /** Initialized */
    GraphicDevice.Initialize(MainAppWnd);
    GraphicDevice.CreateAdditionalSwapChain(SkeletalMeshViewerAppWnd);
    BufferManager->Initialize(GraphicDevice.Device, GraphicDevice.DeviceContext);
    Renderer.Initialize(&GraphicDevice, BufferManager, &GPUTimingManager);
    PrimitiveDrawBatch.Initialize(&GraphicDevice);
    ResourceManager.Initialize(&Renderer, &GraphicDevice);

    GEngine->Init();

    MainUIManager->Initialize(MainAppWnd, GraphicDevice.Device, GraphicDevice.DeviceContext);
    SkeletalMeshViewerUIManager->Initialize(SkeletalMeshViewerAppWnd, GraphicDevice.Device, GraphicDevice.DeviceContext);

    LevelEditor->Initialize(1400, 1000);
    AssetViewer->Initialize(800, 600);
    
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

    // ImGui
    if (MainUIManager && MainUIManager->GetContext() && IsWindowVisible(MainAppWnd))
    {
        MainUIManager->BeginFrame();
        {
            UnrealEditor->Render();

            FConsole::GetInstance().Draw();
            GEngineLoop.EngineProfiler.Render(GraphicDevice.DeviceContext, *GraphicDevice.ScreenWidths.Find(MainAppWnd), *GraphicDevice.ScreenHeights.Find(MainAppWnd));
        }
        MainUIManager->EndFrame();
    }
}

void FEngineLoop::Render(HWND Handle) const
{
    GraphicDevice.Prepare(Handle);

    if (Handle && IsWindowVisible(Handle))
    {
        // ActiveWorld를 변경하여 FRenderer::Render()에서 EditorPreviewWorld를 접근하도록 함
        UWorld* CurrentWorld = GEngine->ActiveWorld;
        if (const UEditorEngine* E = Cast<UEditorEngine>(GEngine))
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
            Renderer.RenderViewport(Handle, AssetViewer->GetActiveViewportClient());
        }

        GEngine->ActiveWorld = CurrentWorld;
    }

    if (Handle && IsWindowVisible(Handle))
    {
        // 스켈레탈 메쉬 뷰어 ImGui
        if (SkeletalMeshViewerUIManager && SkeletalMeshViewerUIManager->GetContext() && Handle == SkeletalMeshViewerAppWnd)
        {
            SkeletalMeshViewerUIManager->BeginFrame();
            UnrealEditor->RenderSubWindowPanel();
            SkeletalMeshViewerUIManager->EndFrame();
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

        MSG Msg;
        while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (Msg.message == WM_QUIT)
            {
                if (Msg.hwnd == MainAppWnd)
                {
                    bIsExit = true;
                    break;
                }
            }
            TranslateMessage(&Msg);
            DispatchMessage(&Msg);
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
        Render(SkeletalMeshViewerAppWnd);

        if (CurrentImGuiContext != nullptr)
        {
            ImGui::SetCurrentContext(CurrentImGuiContext);
        }

        // Delete pending object
        GUObjectArray.ProcessPendingDestroyObjects();

        if (GPUTimingManager.IsInitialized())
        {
            GPUTimingManager.EndFrame();        // End GPU frame timing
        }

        GraphicDevice.SwapBuffer(MainAppWnd);
        
        /** Does not fix errors, This isn't critical error. */
        GraphicDevice.SwapBuffer(SkeletalMeshViewerAppWnd);
        GraphicDevice.SwapBuffer(AnimationViewerAppWnd);
        
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
    /** SkeletalMesh Viewer Section */
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

    /** Main Window Section */
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

HWND FEngineLoop::CreateNewWindow(HINSTANCE hInstance, const WCHAR* WindowClass, const WCHAR* WindowName, int Width, int Height, HWND Parent) const
{

    WNDCLASSEXW wcexSub = {}; // WNDCLASSEXW 사용 권장
    wcexSub.cbSize = sizeof(WNDCLASSEX);
    wcexSub.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; // | CS_DBLCLKS 등 필요시 추가
    wcexSub.lpfnWndProc = AppWndProc;
    wcexSub.cbClsExtra = 0;
    wcexSub.cbWndExtra = 0;
    wcexSub.hInstance = hInstance;
    wcexSub.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcexSub.lpszMenuName = nullptr;
    wcexSub.lpszClassName = WindowClass;

    if (!RegisterClassExW(&wcexSub))
    {
        DWORD Error = GetLastError();
        printf("RegisterClassW failed with error: %d\n", Error);
    }

    HWND hWnd =  CreateWindowExW(
        0, WindowClass, WindowName, WS_POPUP | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, Width, Height,
        nullptr, nullptr, hInstance, nullptr
    );
    
    if (!hWnd)
    {
        DWORD error = GetLastError();
        printf("CreateWindowExW failed with error: %d\n", error);
        return nullptr; // 실패 시 nullptr 반환
    }
    
    return hWnd;
}

LRESULT CALLBACK FEngineLoop::AppWndProc(HWND hWnd, uint32 Msg, WPARAM wParam, LPARAM lParam)
{
    if (hWnd == GEngineLoop.MainAppWnd)
    {
        if (GEngineLoop.MainUIManager)
        {
            ImGui::SetCurrentContext(GEngineLoop.MainUIManager->GetContext());
            if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
            {
                return true;
            }
        }

        switch (Msg)
        {
            case WM_DESTROY:
            {
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
            }
            break;

            case WM_SIZE:
            {
                if (wParam != SIZE_MINIMIZED)
                {
                    if (auto LevelEditor = GEngineLoop.GetLevelEditor())
                    {
                        GraphicDevice.Resize(hWnd);
                
                        uint32 ClientWidth = 0;
                        uint32 ClientHeight = 0;
                        GEngineLoop.GetClientSize(hWnd, ClientWidth, ClientHeight);
            
                        LevelEditor->ResizeEditor(ClientWidth, ClientHeight);
                        
                        Renderer.TileLightCullingPass->ResizeViewBuffers(
                            static_cast<uint32>(LevelEditor->GetActiveViewportClient()->GetD3DViewport().Width),
                            static_cast<uint32>(LevelEditor->GetActiveViewportClient()->GetD3DViewport().Height)
                        );
                    }
                    
                    GEngineLoop.UpdateUI(hWnd);
                }
            }
            break;

            case WM_ACTIVATE:
            {
                if (ImGui::GetCurrentContext() == nullptr)
                {
                    break;
                }

                ImGui::SetCurrentContext(GEngineLoop.MainUIManager->GetContext());
                GEngineLoop.CurrentImGuiContext = ImGui::GetCurrentContext();
            }

            default:
            {
                if (GEngineLoop.AppMessageHandler != nullptr)
                {
                    GEngineLoop.AppMessageHandler->ProcessMessage(hWnd, Msg, wParam, lParam);
                }
            }
            return DefWindowProc(hWnd, Msg, wParam, lParam);
        }
        
        return 0;
    }

    if (hWnd == GEngineLoop.SkeletalMeshViewerAppWnd)
    {
        if (GEngineLoop.SkeletalMeshViewerUIManager)
        {
            ImGui::SetCurrentContext(GEngineLoop.SkeletalMeshViewerUIManager->GetContext());
            if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
            {
                return true;
            }
        }

        switch (Msg)
        {
            case WM_CLOSE:
            {
                ::ShowWindow(hWnd, SW_HIDE);
            }
            return 0;

            case WM_SIZE:
            {
                if (wParam != SIZE_MINIMIZED)
                {
                    if (auto AssetViewer = GEngineLoop.GetAssetViewer())
                    {
                        GraphicDevice.Resize(hWnd);
                        
                        uint32 ClientWidth = 0;
                        uint32 ClientHeight = 0;
                        GEngineLoop.GetClientSize(hWnd, ClientWidth, ClientHeight);
                        
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
                    GEngineLoop.UpdateUI(hWnd, true);
                }
            }
            return 0;

            case WM_ACTIVATE:
            {
                if (ImGui::GetCurrentContext() == nullptr)
                {
                    break;
                }
                ImGui::SetCurrentContext(GEngineLoop.SkeletalMeshViewerUIManager->GetContext());
                GEngineLoop.CurrentImGuiContext = ImGui::GetCurrentContext();
            }
            return 0;
        
            default:
            {
                GEngineLoop.AppMessageHandler->ProcessMessage(hWnd, Msg, wParam, lParam);
            }
            break;
        }
    }
    return DefWindowProc(hWnd, Msg, wParam, lParam);
}

LRESULT FEngineLoop::SubAppWndProc(HWND hWnd, uint32 Msg, WPARAM wParam, LPARAM lParam)
{
    // 이전 ImGui 컨텍스트 저장
    ImGuiContext* PreviousContext = ImGui::GetCurrentContext();
    
    if (GEngineLoop.SkeletalMeshViewerUIManager)
    {
        // 서브 윈도우 컨텍스트로 임시 설정하고 이벤트 처리
        ImGui::SetCurrentContext(GEngineLoop.SkeletalMeshViewerUIManager->GetContext());
        GEngineLoop.CurrentImGuiContext = ImGui::GetCurrentContext();
        if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
        {
            // 이벤트 처리 후 이전 컨텍스트 복원
            ImGui::SetCurrentContext(PreviousContext);
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
        break;
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        ImGui::SetCurrentContext(PreviousContext); // 컨텍스트 복원 후 반환
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
        // 활성화 상태일 때만 컨텍스트 업데이트 (WA_ACTIVE=1, WA_CLICKACTIVE=2)
        else if (wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE)
        {
            // SubAppWnd가 활성화될 때 ImGui 컨텍스트를 현재 윈도우용으로 유지
            // 단, 이전 컨텍스트를 변경하지 않도록 GEngineLoop.CurrentImGuiContext만 업데이트
            GEngineLoop.CurrentImGuiContext = GEngineLoop.SkeletalMeshViewerUIManager->GetContext();
        }
        break;
    default:
        // MessageHandler 처리
        GEngineLoop.AppMessageHandler->ProcessMessage(hWnd, Msg, wParam, lParam);
        break;
    }

    LRESULT Result = DefWindowProc(hWnd, Msg, wParam, lParam);
    
    // 이전 컨텍스트로 복원
    ImGui::SetCurrentContext(PreviousContext);
    
    return Result;
}

void FEngineLoop::UpdateUI(HWND hWnd, bool bSubWindow) const
{
    FConsole::GetInstance().OnResize(MainAppWnd);
    
    if (GEngineLoop.GetUnrealEditor())
    {
        GEngineLoop.GetUnrealEditor()->OnResize(hWnd, bSubWindow);
    }
    
    ViewportTypePanel::GetInstance().OnResize(hWnd);
}

void FEngineLoop::ToggleWindow(const HWND hWnd)
{
    if (hWnd && !IsWindowVisible(hWnd))
    {
        ShowWindow(hWnd, SW_SHOWDEFAULT);
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
