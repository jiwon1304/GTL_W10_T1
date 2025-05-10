#include "EngineLoop.h"
#include "ImGuiManager.h"
#include "UnrealClient.h"
#include "WindowsPlatformTime.h"
#include "D3D11RHI/GraphicDevice.h"
#include "Engine/EditorEngine.h"
#include "LevelEditor/SLevelEditor.h"
#include "Viewer/SlateViewer.h"
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
TMap<HWND, ImGuiContext*> FEngineLoop::WndImGuiContextMap = {};

FEngineLoop::FEngineLoop()
    : MainAppWnd(nullptr)
    , SkeletalMeshViewerAppWnd(nullptr)
    , AnimationViewerAppWnd(nullptr)
    , MainUIManager(nullptr)
    , SkeletalMeshViewerUIManager(nullptr)
    , AnimationViewerUIManager(nullptr)
    , CurrentImGuiContext(nullptr)
    , LevelEditor(nullptr)
    , SkeletalMeshViewer(nullptr)
    , AnimationViewer(nullptr)
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

    AnimationViewerAppWnd = CreateNewWindow(hInstance, L"AnimationWindowClass", L"Animation Viewer", 800, 600, nullptr);
    
    /** New Constructor */
    BufferManager = new FDXDBufferManager();
    MainUIManager = new UImGuiManager;
    SkeletalMeshViewerUIManager = new UImGuiManager;
    AnimationViewerUIManager = new UImGuiManager;
    LevelEditor = new SLevelEditor();
    SkeletalMeshViewer = new SlateViewer();
    AnimationViewer = new SlateViewer();
    UnrealEditor = new UnrealEd();
    AppMessageHandler = std::make_unique<FSlateAppMessageHandler>();
    GEngine = FObjectFactory::ConstructObject<UEditorEngine>(nullptr);

    /** Initialized */
    GraphicDevice.Initialize(MainAppWnd);
    GraphicDevice.CreateAdditionalSwapChain(SkeletalMeshViewerAppWnd);
    GraphicDevice.CreateAdditionalSwapChain(AnimationViewerAppWnd);
    
    BufferManager->Initialize(GraphicDevice.Device, GraphicDevice.DeviceContext);
    Renderer.Initialize(&GraphicDevice, BufferManager, &GPUTimingManager);
    PrimitiveDrawBatch.Initialize(&GraphicDevice);
    ResourceManager.Initialize(&Renderer, &GraphicDevice);

    GEngine->Init();

    MainUIManager->Initialize(MainAppWnd, GraphicDevice.Device, GraphicDevice.DeviceContext);
    SkeletalMeshViewerUIManager->Initialize(SkeletalMeshViewerAppWnd, GraphicDevice.Device, GraphicDevice.DeviceContext);
    AnimationViewerUIManager->Initialize(AnimationViewerAppWnd, GraphicDevice.Device, GraphicDevice.DeviceContext);

    WndImGuiContextMap.Add(MainAppWnd, MainUIManager->GetContext());
    WndImGuiContextMap.Add(SkeletalMeshViewerAppWnd, SkeletalMeshViewerUIManager->GetContext());
    WndImGuiContextMap.Add(AnimationViewerAppWnd, AnimationViewerUIManager->GetContext());
    
    LevelEditor->Initialize(1400, 1000);
    SkeletalMeshViewer->Initialize(SkeletalMeshViewerAppWnd, "SkeletalMeshViewer.ini", 800, 600);
    AnimationViewer->Initialize(AnimationViewerAppWnd, "AnimationViewer.ini", 800, 600);
    
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
        EngineProfiler.RegisterStatScope(TEXT("|- SkeletalMeshPass"), FName(TEXT("SkeletalMeshPass_CPU")), FName(TEXT("SkeletalMeshPass_GPU")));
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
        SlateViewer* Viewer = nullptr;
        if (Handle == SkeletalMeshViewerAppWnd)
        {
            Viewer = SkeletalMeshViewer;
        }
        else if (Handle == AnimationViewerAppWnd)
        {
            Viewer = AnimationViewer;
        }
        
        // ActiveWorld를 변경하여 FRenderer::Render()에서 EditorPreviewWorld를 접근하도록 함
        UWorld* CurrentWorld = GEngine->ActiveWorld;
        if (const UEditorEngine* E = Cast<UEditorEngine>(GEngine))
        {
            UWorld* EditorWorld = E->EditorPreviewWorld;
            if (EditorWorld)
            {
                GEngine->ActiveWorld = EditorWorld;
                Renderer.Render(Viewer->GetActiveViewportClient());
                auto Viewport = Viewer->GetActiveViewportClient();
                auto Location = Viewport->GetCameraLocation();

                // UE_LOG(ELogLevel::Display, TEXT("%f %f %f"), Location.X, Location.Y, Location.Z);
            }
            Renderer.RenderViewport(Handle, Viewer->GetActiveViewportClient());
        }

        GEngine->ActiveWorld = CurrentWorld;
    }

    if (Handle && IsWindowVisible(Handle))
    {
        // 스켈레탈 메쉬 뷰어 ImGui
        if (SkeletalMeshViewerUIManager && SkeletalMeshViewerUIManager->GetContext() && Handle == SkeletalMeshViewerAppWnd)
        {
            SkeletalMeshViewerUIManager->BeginFrame();
            UnrealEditor->RenderSubWindowPanel(Handle);
            SkeletalMeshViewerUIManager->EndFrame();
        }

        // Animation Mesh Viewer
        if (AnimationViewerUIManager && AnimationViewerUIManager->GetContext() && Handle == AnimationViewerAppWnd)
        {
            AnimationViewerUIManager->BeginFrame();
            UnrealEditor->RenderSubWindowPanel(Handle);
            AnimationViewerUIManager->EndFrame();
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
        SkeletalMeshViewer->Tick(DeltaTime);
        AnimationViewer->Tick(DeltaTime);
        // @todo SkeletalMeshViewer->Tick(DeltaTime);

        /* Render Viewports */
        Render();
        Render(SkeletalMeshViewerAppWnd);
        Render(AnimationViewerAppWnd);

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
    if (SkeletalMeshViewer)
    {
        SkeletalMeshViewer->Release();
        delete SkeletalMeshViewer;
        SkeletalMeshViewer = nullptr;
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

    /** Animation Viewer Section */
    if (AnimationViewer)
    {
        AnimationViewer->Release();
        delete AnimationViewer;
        AnimationViewer = nullptr;
    }
    
    if (AnimationViewerAppWnd && IsWindow(AnimationViewerAppWnd))
    {
        DestroyWindow(AnimationViewerAppWnd);
        AnimationViewerAppWnd = nullptr;
    }

    if (AnimationViewerUIManager)
    {
        AnimationViewerUIManager->Shutdown();
        delete AnimationViewerUIManager;
        AnimationViewerUIManager = nullptr;
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
    /** ImContext Proc */
    ImGuiContext* UpdateContext = WndImGuiContextMap[hWnd];
    if (UpdateContext)
    {
        ImGui::SetCurrentContext(UpdateContext);
        if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
        {
            return true;
        }
    }
    
    if (hWnd == GEngineLoop.MainAppWnd)
    {
        switch (Msg)
        {
            case WM_DESTROY:
            {
                PostQuitMessage(0);
                if (auto LevelEditor = GEngineLoop.GetLevelEditor())
                {
                    LevelEditor->SaveConfig();
                }
                if (auto AssetViewer = GEngineLoop.GetSkeletalMeshViewer())
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

                ImGui::SetCurrentContext(UpdateContext);
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
    }
    
    switch (Msg)
    {
        case WM_SIZE:
        {
            if (wParam != SIZE_MINIMIZED)
            {
                GraphicDevice.Resize(hWnd);

                uint32 ClientWidth = 0;
                uint32 ClientHeight = 0;
                GEngineLoop.GetClientSize(hWnd, ClientWidth, ClientHeight);
                
                GEngineLoop.UpdateUI(hWnd, true);
                
                if (hWnd == GEngineLoop.SkeletalMeshViewerAppWnd)
                {
                    if (SlateViewer* SkeletalViewer = GEngineLoop.GetSkeletalMeshViewer())
                    {
                        SkeletalViewer->ResizeEditor(ClientWidth, ClientHeight);
                        if (SkeletalViewer->GetActiveViewportClient())
                        {
                            // 필요하다면 TileLightCullingPass 버퍼도 리사이즈
                            Renderer.TileLightCullingPass->ResizeViewBuffers(
                                static_cast<uint32>(SkeletalViewer->GetActiveViewportClient()->GetD3DViewport().Width),
                                static_cast<uint32>(SkeletalViewer->GetActiveViewportClient()->GetD3DViewport().Height)
                            );
                        }
                    }
                }

                if (hWnd == GEngineLoop.AnimationViewerAppWnd)
                {
                    if (SlateViewer* AnimationViewer = GEngineLoop.GetAnimationViewer())
                    {
                        AnimationViewer->ResizeEditor(ClientWidth, ClientHeight);
                        if (AnimationViewer->GetActiveViewportClient())
                        {
                            
                            Renderer.TileLightCullingPass->ResizeViewBuffers(
                                static_cast<uint32>(AnimationViewer->GetActiveViewportClient()->GetD3DViewport().Width),
                                static_cast<uint32>(AnimationViewer->GetActiveViewportClient()->GetD3DViewport().Height)
                            );
                        }
                    }
                }
            }
        }
        return 0;
        
        case WM_CLOSE:
        {
            ::ShowWindow(hWnd, SW_HIDE);
        }
        return 0;
    
        case WM_ACTIVATE:
        {
            if (ImGui::GetCurrentContext() == nullptr)
            {
                break;
            }
            ImGui::SetCurrentContext(UpdateContext);
            GEngineLoop.CurrentImGuiContext = ImGui::GetCurrentContext();
        }
        return 0;
            
        default:
        {
            GEngineLoop.AppMessageHandler->ProcessMessage(hWnd, Msg, wParam, lParam);
        }
        break;
    }

    return DefWindowProc(hWnd, Msg, wParam, lParam);
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

void FEngineLoop::ToggleWindow(const HWND hWnd) const
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

void FEngineLoop::Show(const HWND HWnd) const
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

void FEngineLoop::Hide(const HWND hWnd) const
{
    if (hWnd)
    {
        ShowWindow(hWnd, SW_HIDE);
    }
}
