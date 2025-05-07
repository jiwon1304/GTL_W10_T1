#include "AssetViewer.h"

#include "UObject/Object.h"
#include "EngineLoop.h" // Assuming Launch module, adjust if needed
#include "UnrealClient.h"
#include "WindowsCursor.h"
#include "Widgets/SWindow.h" // Assuming SlateCore module
#include "Widgets/Layout/SSplitter.h"
#include "UnrealEd/EditorViewportClient.h"
#include "SlateCore/Input/Events.h"

extern FEngineLoop GEngineLoop;

SAssetViewer::SAssetViewer()
    : PrimaryVSplitter(nullptr)
    , CenterAndRightVSplitter(nullptr)
    , RightSidebarHSplitter(nullptr)
    , EditorWidth(800)
    , EditorHeight(600)
{
}

void SAssetViewer::Initialize(uint32 InEditorWidth, uint32 InEditorHeight)
{
    EditorWidth = InEditorWidth;
    EditorHeight = InEditorHeight;

    // Initialize Splitters
    // 0.2 : 0.6 : 0.2 => 0.2 : 0.8 ( 0.75 : 0.25 )
    PrimaryVSplitter = new SSplitterV();
    PrimaryVSplitter->SplitRatio = 0.2f;
    PrimaryVSplitter->Initialize(FRect(0, 0, static_cast<float>(InEditorWidth), static_cast<float>(InEditorHeight)));

    FRect CenterAndRightAreaRect = PrimaryVSplitter->SideRB->GetRect();
    CenterAndRightVSplitter = new SSplitterV();
    CenterAndRightVSplitter->SplitRatio = 0.75f;
    CenterAndRightVSplitter->Initialize(CenterAndRightAreaRect);

    FRect RightSidebarAreaRect = CenterAndRightVSplitter->SideRB->GetRect();
    RightSidebarHSplitter = new SSplitterH();
    RightSidebarHSplitter->SplitRatio = 0.3f;
    RightSidebarHSplitter->Initialize(RightSidebarAreaRect);

    // Create and initialize the viewport client
    ActiveViewportClient = std::make_shared<FEditorViewportClient>();
    if (ActiveViewportClient)
    {
        // If FEditorViewportClient has an Initialize method, call it here.
        EViewScreenLocation Location = EViewScreenLocation::EVL_MAX;
        FRect Rect = CenterAndRightVSplitter->SideLT->GetRect();
        ActiveViewportClient->Initialize(Location, Rect);
        // For now, we assume basic setup is done in constructor or will be handled by SetRect/ResizeViewport.
        // If your FEditorViewportClient requires a UWorld or other specific setup:
        // UWorld* PreviewWorld = GEngine->GetEditorWorldContext().World(); // Or a dedicated preview world
        // ActiveViewportClient->SetPreviewWorld(PreviewWorld);
    }

    LoadConfig();

    RegisterViewerInputDelegates();
}

void SAssetViewer::Release()
{
    ActiveViewportClient.reset();

    // Delete SWindow objects held by splitters before deleting the splitters themselves,
    // assuming SSplitter destructors do not delete SWindows they don't own (especially nested splitters).
    if (RightSidebarHSplitter)
    {
        if (RightSidebarHSplitter->SideLT && RightSidebarHSplitter->SideLT != CenterAndRightVSplitter && RightSidebarHSplitter->SideLT != PrimaryVSplitter) delete RightSidebarHSplitter->SideLT; RightSidebarHSplitter->SideLT = nullptr;
        if (RightSidebarHSplitter->SideRB && RightSidebarHSplitter->SideRB != CenterAndRightVSplitter && RightSidebarHSplitter->SideRB != PrimaryVSplitter) delete RightSidebarHSplitter->SideRB; RightSidebarHSplitter->SideRB = nullptr;
        delete RightSidebarHSplitter; RightSidebarHSplitter = nullptr;
    }
    if (CenterAndRightVSplitter)
    {
        if (CenterAndRightVSplitter->SideLT && CenterAndRightVSplitter->SideLT != PrimaryVSplitter) delete CenterAndRightVSplitter->SideLT; CenterAndRightVSplitter->SideLT = nullptr;
        // CenterAndRightVSplitter->SideRB was RightSidebarHSplitter, already deleted if logic is top-down.
        // Or, if SSplitter handles its direct SWindow children but not nested splitters, this is fine.
        // For safety, ensure SideRB is nulled if it pointed to a now-deleted splitter.
        CenterAndRightVSplitter->SideRB = nullptr;
        delete CenterAndRightVSplitter; CenterAndRightVSplitter = nullptr;
    }
    if (PrimaryVSplitter)
    {
        delete PrimaryVSplitter->SideLT; PrimaryVSplitter->SideLT = nullptr;
        // PrimaryVSplitter->SideRB was CenterAndRightVSplitter, already deleted.
        PrimaryVSplitter->SideRB = nullptr;
        delete PrimaryVSplitter; PrimaryVSplitter = nullptr;
    }
}

void SAssetViewer::Tick(float DeltaTime)
{
    if (ActiveViewportClient && CenterAndRightVSplitter && CenterAndRightVSplitter->SideLT)
    {
        FRect ViewportRect = CenterAndRightVSplitter->SideLT->GetRect();
        ActiveViewportClient->GetViewport()->ResizeViewport(ViewportRect);
        ActiveViewportClient->Tick(DeltaTime);
    }
}

void SAssetViewer::ResizeEditor(uint32 InEditorWidth, uint32 InEditorHeight)
{
    if (InEditorWidth == EditorWidth && InEditorHeight == EditorHeight)
    {
        return;
    }

    EditorWidth = InEditorWidth;
    EditorHeight = InEditorHeight;

    // Primary 스플리터 부터 리사이즈 전파
    if (PrimaryVSplitter)
    {
        //PrimaryVSplitter->SetRect(FRect(0, 0, static_cast<float>(InEditorWidth), static_cast<float>(InEditorHeight)));
        PrimaryVSplitter->OnResize(InEditorWidth, InEditorHeight);

        FRect CenterAndRightAreaRect = PrimaryVSplitter->SideRB->GetRect();
        if (CenterAndRightVSplitter)
        {
            CenterAndRightVSplitter->SetRect(CenterAndRightAreaRect);
            CenterAndRightVSplitter->OnResize(static_cast<uint32>(CenterAndRightAreaRect.Width), static_cast<uint32>(CenterAndRightAreaRect.Height));

            FRect RightSidebarAreaRect = CenterAndRightVSplitter->SideRB->GetRect();
            if (RightSidebarHSplitter)
            {
                RightSidebarHSplitter->SetRect(RightSidebarAreaRect);
            }
        }

        ResizeViewport();
    }

}

void SAssetViewer::ResizeViewport()
{
    if (PrimaryVSplitter)
    {
        FRect ViewportAreaRect = CenterAndRightVSplitter->SideLT->GetRect();
        if (ActiveViewportClient && ActiveViewportClient->GetViewport() && CenterAndRightVSplitter && CenterAndRightVSplitter->SideLT)
        {
            const FRect FullRect(0.f, 72.f, ViewportAreaRect.Width, ViewportAreaRect.Height - 72.f - 32.f);
            ActiveViewportClient->GetViewport()->ResizeViewport(FullRect);
        }
    }
}

void SAssetViewer::SelectViewport(const FVector2D& Point)
{
    if (ActiveViewportClient && CenterAndRightVSplitter && CenterAndRightVSplitter->SideLT)
    {
        if (ActiveViewportClient->IsSelected(Point))
        {
            // 뷰포트 선택 시 키 상태 초기화
            ActiveViewportClient->ResetKeyState();
            
            // 뷰포트에 포커스 설정
            SetFocus(GEngineLoop.SkeletalMeshViewerAppWnd);
        }
    }
}

void SAssetViewer::RegisterViewerInputDelegates()
{
    FSlateAppMessageHandler* Handler = GEngineLoop.GetAppMessageHandler();

    // 기존 델리게이트 제거
    for (const FDelegateHandle& Handle : InputDelegatesHandles)
    {
        Handler->OnKeyCharDelegate.Remove(Handle);
        Handler->OnKeyDownDelegate.Remove(Handle);
        Handler->OnKeyUpDelegate.Remove(Handle);
        Handler->OnMouseDownDelegate.Remove(Handle);
        Handler->OnMouseUpDelegate.Remove(Handle);
        Handler->OnMouseDoubleClickDelegate.Remove(Handle);
        Handler->OnMouseWheelDelegate.Remove(Handle);
        Handler->OnMouseMoveDelegate.Remove(Handle);
        Handler->OnRawMouseInputDelegate.Remove(Handle);
        Handler->OnRawKeyboardInputDelegate.Remove(Handle);
    }
    
    //InputDelegatesHandles.Empty();

    InputDelegatesHandles.Add(Handler->OnKeyDownDelegate.AddLambda([this](HWND hWnd, const FKeyEvent& InKeyEvent)
    {
        if (hWnd != GEngineLoop.SkeletalMeshViewerAppWnd)
        {
            return;
        }
        
        if (ImGui::GetIO().WantCaptureKeyboard)
        {
            return;
        }
        if (ActiveViewportClient)
        {
            ActiveViewportClient->InputKey(InKeyEvent);
        }
    }));
    
    InputDelegatesHandles.Add(Handler->OnKeyUpDelegate.AddLambda([this](HWND hWnd, const FKeyEvent& InKeyEvent)
    {
        if (hWnd != GEngineLoop.SkeletalMeshViewerAppWnd)
        {
            return;
        }
        
        if (ImGui::GetIO().WantCaptureKeyboard)
        {
            return;
        }
        if (ActiveViewportClient)
        {
            ActiveViewportClient->InputKey(InKeyEvent);
        }
    }));
    
    InputDelegatesHandles.Add(Handler->OnMouseDownDelegate.AddLambda([this](HWND hWnd, const FPointerEvent& InMouseEvent)
    {
        if (hWnd != GEngineLoop.SkeletalMeshViewerAppWnd)
        {
            return;
        }
        
        if (ImGui::GetIO().WantCaptureMouse)
        {
            return;
        }

        switch (InMouseEvent.GetEffectingButton())  // NOLINT(clang-diagnostic-switch-enum)
        {
        case EKeys::LeftMouseButton:
            {
                // @todo 피킹 로직 헬퍼 구현
                break;
            }
        case EKeys::RightMouseButton:
            {
                if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
                {
                    FWindowsCursor::SetShowMouseCursor(false);
                    MousePinPosition = InMouseEvent.GetScreenSpacePosition();

                    // 마우스 우클릭 상태 설정
                    if (ActiveViewportClient)
                    {
                        ActiveViewportClient->SetRightMouseDown(true);
                    }
                }
                break;
            }
        default:
            break;
        }
        
        POINT Point;
        GetCursorPos(&Point);
        ScreenToClient(hWnd, &Point);
        const FVector2D ClientPos = FVector2D{ static_cast<float>(Point.x), static_cast<float>(Point.y) };

        PrimaryVSplitter->OnPressed({ ClientPos.X, ClientPos.Y });
        CenterAndRightVSplitter->OnPressed({ ClientPos.X, ClientPos.Y });
        RightSidebarHSplitter->OnPressed({ ClientPos.X, ClientPos.Y });
        
        SelectViewport(ClientPos);
    }));
    
    InputDelegatesHandles.Add(Handler->OnMouseUpDelegate.AddLambda([this](HWND hWnd, const FPointerEvent& InMouseEvent)
    {
        if (hWnd != GEngineLoop.SkeletalMeshViewerAppWnd)
        {
            return;
        }
        
        switch (InMouseEvent.GetEffectingButton())  // NOLINT(clang-diagnostic-switch-enum)
        {
        case EKeys::RightMouseButton:
        {
            // 우클릭 해제 시 상태 초기화
            if (ActiveViewportClient)
            {
                ActiveViewportClient->SetRightMouseDown(false);
            }

            FWindowsCursor::SetShowMouseCursor(true);
            FWindowsCursor::SetPosition(
                static_cast<int32>(MousePinPosition.X),
                static_cast<int32>(MousePinPosition.Y)
            );
            return;
        }
        // Viewport 선택 및 스플리터 해제 로직
        case EKeys::LeftMouseButton:
        {
            // 모든 스플리터 해제
            if (PrimaryVSplitter)
            {
                PrimaryVSplitter->OnReleased();
            }
            if (CenterAndRightVSplitter)
            {
                CenterAndRightVSplitter->OnReleased();
            }
            if (RightSidebarHSplitter)
            {
                RightSidebarHSplitter->OnReleased();
            }
            return;
        }

        default:
            return;
        }
    }));

    InputDelegatesHandles.Add(Handler->OnMouseMoveDelegate.AddLambda([this](HWND hWnd, const FPointerEvent& InMouseEvent)
    {
        if (hWnd != GEngineLoop.SkeletalMeshViewerAppWnd)
        {
            return;
        }
        
        if (ImGui::GetIO().WantCaptureMouse)
        {
            return;
        }

        // Splitter 움직임 로직
        if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
        {
            const auto& [DeltaX, DeltaY] = InMouseEvent.GetCursorDelta();

            bool bIsSplitterDragging = false;
            if (PrimaryVSplitter && PrimaryVSplitter->IsSplitterPressed())
            {
                PrimaryVSplitter->OnDrag(FPoint(DeltaX, DeltaY));
                bIsSplitterDragging = true;
            }
            if (CenterAndRightVSplitter && CenterAndRightVSplitter->IsSplitterPressed())
            {
                CenterAndRightVSplitter->OnDrag(FPoint(DeltaX, DeltaY));
                bIsSplitterDragging = true;
            }
            if (RightSidebarHSplitter && RightSidebarHSplitter->IsSplitterPressed())
            {
                RightSidebarHSplitter->OnDrag(FPoint(DeltaX, DeltaY));
                bIsSplitterDragging = true;
            }

            if (bIsSplitterDragging)
            {
                if (PrimaryVSplitter)
                {
                    FRect CentralRightAreaRect = PrimaryVSplitter->SideRB->GetRect();
                    if (CenterAndRightVSplitter)
                    {
                        CenterAndRightVSplitter->SetRect(CentralRightAreaRect);
                        CenterAndRightVSplitter->OnResize(static_cast<uint32>(CentralRightAreaRect.Width), static_cast<uint32>(CentralRightAreaRect.Height));

                        FRect RightSidebarAreaRect = CenterAndRightVSplitter->SideRB->GetRect();
                        if (RightSidebarHSplitter)
                        {
                            RightSidebarHSplitter->SetRect(RightSidebarAreaRect);
                        }
                    }
                }

                ResizeViewport();
            }
        }

        if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && !InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
        {
            ECursorType CursorType = ECursorType::Arrow;
            POINT Point;
            GetCursorPos(&Point);
            FVector2D MousePos = FVector2D{ static_cast<float>(Point.x), static_cast<float>(Point.y) };
            //ScreenToClient(GEngineLoop.MainAppWnd, &Point);
            //FVector2D ClientPos = FVector2D{ static_cast<float>(Point.x), static_cast<float>(Point.y) };

            // 모든 스플리터에 대해 Hover 검사
            bool bPrimaryHovered = PrimaryVSplitter->IsSplitterHovered({ MousePos.X, MousePos.Y });
            bool bCentralRightHovered = CenterAndRightVSplitter->IsSplitterHovered({ MousePos.X, MousePos.Y });
            bool bRightSidebarHovered = RightSidebarHSplitter->IsSplitterHovered({ MousePos.X, MousePos.Y });
            if (bPrimaryHovered)
            {
                CursorType = ECursorType::ResizeLeftRight;
            }
            else if (bCentralRightHovered)
            {
                CursorType = ECursorType::ResizeLeftRight;
            }
            else if (bRightSidebarHovered)
            {
                CursorType = ECursorType::ResizeUpDown;
            }

            FWindowsCursor::SetMouseCursor(CursorType);
        }
    }));

    InputDelegatesHandles.Add(Handler->OnRawMouseInputDelegate.AddLambda([this](HWND hWnd, const FPointerEvent& InMouseEvent)
    {
        if (hWnd != GEngineLoop.SkeletalMeshViewerAppWnd)
        {
            return;
        }
        
        // Mouse Move 이벤트 일때만 실행
        if (InMouseEvent.GetInputEvent() == IE_Axis
         && InMouseEvent.GetEffectingButton() == EKeys::Invalid)
        {
            // 에디터 카메라 이동 로직
            if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton)
              && InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
            {
                if (ActiveViewportClient)
                {
                    ActiveViewportClient->MouseMove(InMouseEvent);
                }
            }
            // @todo Gizmo 조작
        }
        // 마우스 휠 이벤트
        else if (InMouseEvent.GetEffectingButton() == EKeys::MouseWheelAxis)
        {
            // 카메라 속도 조절
            if (InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && ActiveViewportClient->IsPerspective())
            {
                const float CurrentSpeed = ActiveViewportClient->GetCameraSpeedScalar();
                const float Adjustment = FMath::Sign(InMouseEvent.GetWheelDelta()) * FMath::Loge(CurrentSpeed + 1.0f) * 0.5f;

                ActiveViewportClient->SetCameraSpeed(CurrentSpeed + Adjustment);
            }
        }
    }));
    
    InputDelegatesHandles.Add(Handler->OnMouseWheelDelegate.AddLambda([this](HWND hWnd, const FPointerEvent& InMouseEvent)
    {
        if (hWnd != GEngineLoop.SkeletalMeshViewerAppWnd)
        {
            return;
        }
        
        if (ImGui::GetIO().WantCaptureMouse)
        {
            return;
        }

        if (ActiveViewportClient)
        {
            // 뷰포트에서 앞뒤 방향으로 화면 이동
            if (ActiveViewportClient->IsPerspective())
            {
                if (!InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
                {
                    const FVector CameraLoc = ActiveViewportClient->PerspectiveCamera.GetLocation();
                    const FVector CameraForward = ActiveViewportClient->PerspectiveCamera.GetForwardVector();
                    ActiveViewportClient->PerspectiveCamera.SetLocation(
                        CameraLoc + CameraForward * InMouseEvent.GetWheelDelta() * 50.0f
                    );
                }
            }
            else
            {
                FEditorViewportClient::SetOthoSize(-InMouseEvent.GetWheelDelta());
            }
        }
    }));
}

void SAssetViewer::LoadConfig()
{
    TMap<FString, FString> Config = ReadIniFile(IniFilePath);
    if (Config.Num() == 0) return;

    ActiveViewportClient->LoadConfig(Config);

    if (PrimaryVSplitter)
    {
        PrimaryVSplitter->LoadConfig(Config, TEXT("PrimaryV.SplitRatio"), 0.2f);

        if (CenterAndRightVSplitter)
        {
            FRect CenterAndRightAreaRect = PrimaryVSplitter->SideRB->GetRect();
            CenterAndRightVSplitter->SetRect(CenterAndRightAreaRect);
            CenterAndRightVSplitter->LoadConfig(Config, TEXT("CenterRightV.SplitRatio"), 0.75f);
        }
        if (RightSidebarHSplitter)
        {
            FRect RightSidebarAreaRect = CenterAndRightVSplitter->SideRB->GetRect();
            RightSidebarHSplitter->SetRect(RightSidebarAreaRect);
            RightSidebarHSplitter->LoadConfig(Config, TEXT("RightSidebarH.SplitRatio"), 0.3f);
        }
    }

    ResizeViewport();
}

void SAssetViewer::SaveConfig()
{
    TMap<FString, FString> Config;
    if (PrimaryVSplitter)
    {
        PrimaryVSplitter->SaveConfig(Config, TEXT("PrimaryV.SplitRatio"));
    }
    if (CenterAndRightVSplitter)
    {
        CenterAndRightVSplitter->SaveConfig(Config, TEXT("CenterRightV.SplitRatio"));
    }
    if (RightSidebarHSplitter)
    {
        RightSidebarHSplitter->SaveConfig(Config, TEXT("RightSidebarH.SplitRatio"));
    }

    ActiveViewportClient->SaveConfig(Config);

    if (Config.Num() > 0)
    {
        WriteIniFile(IniFilePath, Config);
    }
}

TMap<FString, FString> SAssetViewer::ReadIniFile(const FString& FilePath)
{
    TMap<FString, FString> ConfigMap;
    std::ifstream IniFile(*FilePath);
    if (!IniFile.is_open())
    {
        return ConfigMap;
    }

    std::string Line;
    while (std::getline(IniFile, Line))
    {
        std::istringstream ISS(Line);
        std::string Key, Value;
        if (std::getline(ISS, Key, '=') && std::getline(ISS, Value))
        {
            Key.erase(0, Key.find_first_not_of(" \t\n\r\f\v"));
            Key.erase(Key.find_last_not_of(" \t\n\r\f\v") + 1);
            Value.erase(0, Value.find_first_not_of(" \t\n\r\f\v"));
            Value.erase(Value.find_last_not_of(" \t\n\r\f\v") + 1);
            ConfigMap.Add(FString(Key.c_str()), FString(Value.c_str()));
        }
    }
    IniFile.close();
    return ConfigMap;
}

void SAssetViewer::WriteIniFile(const FString& FilePath, const TMap<FString, FString>& Config)
{
    std::ofstream IniFile(*FilePath);
    if (!IniFile.is_open())
    {
        UE_LOG(ELogLevel::Error, TEXT("Failed to open INI file for writing: %s"), *FilePath);
        return;
    }

    for (const auto& Pair : Config)
    {
        IniFile << *Pair.Key << "=" << *Pair.Value << '\n';
    }
    IniFile.close();
}
