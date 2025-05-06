#include "SLevelEditor.h"
#include <fstream>
#include <ostream>
#include <sstream>
#include "EngineLoop.h"
#include "UnrealClient.h"
#include "WindowsCursor.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "Engine/EditorEngine.h"
#include "Slate/Widgets/Layout/SSplitter.h"
#include "SlateCore/Widgets/SWindow.h"
#include "UnrealEd/EditorViewportClient.h"

extern FEngineLoop GEngineLoop;

SLevelEditor::SLevelEditor()
    : MainVSplitter(nullptr)        // 메인 수직 스플리터 추가
    , EditorHSplitter(nullptr)      // 편집기 영역용 수평 스플리터
    , ViewportVSplitter(nullptr)    // 뷰포트 영역용 수평 스플리터
    , ViewportHSplitter(nullptr)    // 뷰포트 영역용 수직 스플리터
    , bMultiViewportMode(false)
{
}

void SLevelEditor::Initialize(uint32 InEditorWidth, uint32 InEditorHeight)
{
    // @note Splitter들의 SplitRatio는 LoadConfig의 DefaultValue에 의해 결정됨. 이곳에서 변경하지 말 것.
    MainVSplitter = new SSplitterV();
    MainVSplitter->Initialize(FRect(0.0f, 0.f, static_cast<float>(InEditorWidth), static_cast<float>(InEditorHeight)));

    FRect ViewportAreaRect = MainVSplitter->SideLT->GetRect();
    ViewportHSplitter = new SSplitterH();
    ViewportHSplitter->Initialize(ViewportAreaRect);
    ViewportVSplitter = new SSplitterV();
    ViewportVSplitter->Initialize(ViewportAreaRect);

    FRect PanelAreaRect = MainVSplitter->SideRB->GetRect();
    EditorHSplitter = new SSplitterH();
    EditorHSplitter->Initialize(PanelAreaRect);

    // 각 뷰포트 클라이언트 생성 및 초기화 (기존 로직 활용, 영역 계산 수정)
    FRect TopLeftViewportRect, TopRightViewportRect, BottomLeftViewportRect, BottomRightViewportRect;
    CalculateViewportRects(ViewportAreaRect, TopLeftViewportRect, TopRightViewportRect, BottomLeftViewportRect, BottomRightViewportRect);

    for (size_t i = 0; i < 4; i++)
    {
        EViewScreenLocation Location = static_cast<EViewScreenLocation>(i);
        FRect Rect;
        switch (Location)
        {
        case EViewScreenLocation::EVL_TopLeft:
            Rect = TopLeftViewportRect;
            break;
        case EViewScreenLocation::EVL_TopRight:
            Rect = TopRightViewportRect;
            break;
        case EViewScreenLocation::EVL_BottomLeft:
            Rect = BottomLeftViewportRect;
            break;
        case EViewScreenLocation::EVL_BottomRight:
            Rect = BottomRightViewportRect;
            break;
        case EViewScreenLocation::EVL_MAX:
            Rect = ViewportAreaRect;    // May not happen
            break;
        default:
            return; // Should not happen
        }

        ViewportClients[i] = std::make_shared<FEditorViewportClient>();
        ViewportClients[i]->Initialize(Location, Rect);
    }

    ActiveViewportClient = ViewportClients[0];

    LoadConfig();

    FSlateAppMessageHandler* Handler = GEngineLoop.GetAppMessageHandler();

    Handler->OnPIEModeStartDelegate.AddLambda([this]()
        {
            this->RegisterPIEInputDelegates();
        });

    Handler->OnPIEModeEndDelegate.AddLambda([this]()
        {
            this->RegisterEditorInputDelegates();
        });

    // Register Editor input when first initialization.
    RegisterEditorInputDelegates();
}

void SLevelEditor::Tick(float DeltaTime)
{
    for (const std::shared_ptr<FEditorViewportClient>& Viewport : ViewportClients)
    {
        Viewport->Tick(DeltaTime);
    }
}

void SLevelEditor::Release() const
{
    delete ViewportHSplitter;
    delete ViewportVSplitter;
    delete MainVSplitter; // 메인 스플리터 해제 추가
}

void SLevelEditor::ResizeEditor(const uint32 InEditorWidth, const uint32 InEditorHeight)
{
    if (InEditorWidth == EditorWidth && InEditorHeight == EditorHeight)
    {
        return;
    }

    EditorWidth = InEditorWidth;
    EditorHeight = InEditorHeight;

    // 메인 스플리터부터 리사이즈 전파
    if (MainVSplitter)
    {
        MainVSplitter->OnResize(InEditorWidth, InEditorHeight);

        FRect ViewportAreaRect = MainVSplitter->SideLT->GetRect();
        if (ViewportVSplitter && ViewportHSplitter)
        {
            ViewportVSplitter->SetRect(ViewportAreaRect);
            ViewportHSplitter->SetRect(ViewportAreaRect);
        }

        FRect PanelAreaRect = MainVSplitter->SideRB->GetRect();
        if (EditorHSplitter)
        {
            EditorHSplitter->SetRect(PanelAreaRect);
        }

        ResizeViewports();
    }
}

void SLevelEditor::SelectViewport(const FVector2D& Point)
{
    for (int i = 0; i < 4; i++)
    {
        if (ViewportClients[i]->IsSelected(Point))
        {
            SetActiveViewportClient(i);
            SetFocus(GEngineLoop.MainAppWnd);
            return;
        }
    }
}

// Helper function to calculate individual viewport rects based on the overall viewport area and its internal splitters
void SLevelEditor::CalculateViewportRects(const FRect& ViewportArea, FRect& OutTopLeft, FRect& OutTopRight, FRect& OutBottomLeft, FRect& OutBottomRight) const
{
    if (!ViewportHSplitter || !ViewportVSplitter)
    {
        return;
    }

    // Get the rects defined by the viewport-internal splitters
    const FRect Top = ViewportHSplitter->SideLT->GetRect();
    const FRect Bottom = ViewportHSplitter->SideRB->GetRect();
    const FRect Left = ViewportVSplitter->SideLT->GetRect();
    const FRect Right = ViewportVSplitter->SideRB->GetRect();

    // Calculate the final screen-space rects for each viewport quadrant
    // The splitter rects are relative to the ViewportArea, so we need to add the ViewportArea's origin
    OutTopLeft = FRect(ViewportArea.TopLeftX + Left.TopLeftX, ViewportArea.TopLeftY + Top.TopLeftY, Left.Width, Top.Height);
    OutTopRight = FRect(ViewportArea.TopLeftX + Right.TopLeftX, ViewportArea.TopLeftY + Top.TopLeftY, Right.Width, Top.Height);
    OutBottomLeft = FRect(ViewportArea.TopLeftX + Left.TopLeftX, ViewportArea.TopLeftY + Bottom.TopLeftY, Left.Width, Bottom.Height);
    OutBottomRight = FRect(ViewportArea.TopLeftX + Right.TopLeftX, ViewportArea.TopLeftY + Bottom.TopLeftY, Right.Width, Bottom.Height);
}

// Get the designated area for ImGui panels
FRect SLevelEditor::GetPanelAreaRect() const
{
    // @todo 검사 필요한지 확인 필요
    if (MainVSplitter && MainVSplitter->SideRB)
    {
        return MainVSplitter->SideRB->GetRect();
    }
    return { 0.f, 0.f, 0.f, 0.f }; // Return empty rect if splitter is invalid
}

void SLevelEditor::ResizeViewports()
{
    if (MainVSplitter)
    {
        FRect ViewportAreaRect = MainVSplitter->SideLT->GetRect();

        if (bMultiViewportMode)
        {
            if (GetViewports()[0])
            {
                const FRect Top = ViewportHSplitter->SideLT->GetRect();
                const FRect Bottom = ViewportHSplitter->SideRB->GetRect();
                const FRect Left = ViewportVSplitter->SideLT->GetRect();
                const FRect Right = ViewportVSplitter->SideRB->GetRect();

                for (int i = 0; i < 4; ++i)
                {
                    GetViewports()[i]->ResizeViewport(Top, Bottom, Left, Right);
                }
            }
        }
        else
        {
            if (ActiveViewportClient)
            {
                //const FRect FullRect(0.f, 0.f, ViewportAreaRect.Width, ViewportAreaRect.Height);
                // 72 = Top padding height, 32 = bottom padding height
                const FRect FullRect(0.f, 72.f, ViewportAreaRect.Width * 0.8f, ViewportAreaRect.Height - 72.f - 32.f);
                ActiveViewportClient->GetViewport()->ResizeViewport(FullRect); // 임시: Top=Bottom=Left=Right=전체영역
                // TODO: FEditorViewportClient::ResizeViewport에서 단일 뷰포트 모드 처리 로직 확인/수정 필요
            }
        }
    }
}

void SLevelEditor::SetEnableMultiViewport(const bool bIsEnable)
{
    bMultiViewportMode = bIsEnable;
    ResizeViewports();
}

bool SLevelEditor::IsMultiViewport() const
{
    return bMultiViewportMode;
}

void SLevelEditor::LoadConfig()
{
    auto Config = ReadIniFile(IniFilePath);

    int32 WindowX = FMath::Max(GetValueFromConfig(Config, "WindowX", 0), 0);
    int32 WindowY = FMath::Max(GetValueFromConfig(Config, "WindowY", 0), 0);
    int32 WindowWidth = GetValueFromConfig(Config, "WindowWidth", EditorWidth);
    int32 WindowHeight = GetValueFromConfig(Config, "WindowHeight", EditorHeight);
    if (WindowWidth > 100 && WindowHeight > 100)
    {
        MoveWindow(GEngineLoop.MainAppWnd, WindowX, WindowY, WindowWidth, WindowHeight, true);
    }
    const bool bIsZoomed = GetValueFromConfig(Config, "Zoomed", false);
    if (bIsZoomed)
    {
        ShowWindow(GEngineLoop.MainAppWnd, SW_MAXIMIZE);
    }

    FEditorViewportClient::Pivot.X = GetValueFromConfig(Config, "OrthoPivotX", 0.0f);
    FEditorViewportClient::Pivot.Y = GetValueFromConfig(Config, "OrthoPivotY", 0.0f);
    FEditorViewportClient::Pivot.Z = GetValueFromConfig(Config, "OrthoPivotZ", 0.0f);
    FEditorViewportClient::OrthoSize = GetValueFromConfig(Config, "OrthoZoomSize", 10.0f);

    SetActiveViewportClient(GetValueFromConfig(Config, "ActiveViewportIndex", 0));
    bMultiViewportMode = GetValueFromConfig(Config, "bMultiView", false);
    if (bMultiViewportMode)
    {
        SetEnableMultiViewport(true);
    }
    else
    {
        SetEnableMultiViewport(false);
    }

    for (const auto& ViewportClient : ViewportClients)
    {
        ViewportClient->LoadConfig(Config);
    }

    // @todo 더 깔끔한 LoadConfig 구현 필요
    if (MainVSplitter)
    {
        MainVSplitter->LoadConfig(Config, "MainSplitterV.SplitRatio", 0.8f);

        if (ViewportVSplitter && ViewportHSplitter)
        {
            FRect ViewportAreaRect = MainVSplitter->SideLT->GetRect();
            ViewportVSplitter->SetRect(ViewportAreaRect);
            ViewportHSplitter->SetRect(ViewportAreaRect);
            ViewportVSplitter->LoadConfig(Config, "ViewportSplitterV.SplitRatio", 0.5f);
            ViewportHSplitter->LoadConfig(Config, "ViewportSplitterH.SplitRatio", 0.5f);
        }
        if (EditorHSplitter)
        {
            FRect PanelAreaRect = MainVSplitter->SideRB->GetRect();
            EditorHSplitter->SetRect(PanelAreaRect);
            EditorHSplitter->LoadConfig(Config, "EditorSplitterH.SplitRatio", 0.3f);
        }
    }
    ResizeViewports();
}

void SLevelEditor::SaveConfig()
{
    TMap<FString, FString> Config;
    // @todo 더 깔끔한 SaveConfig 구현 필요
    if (MainVSplitter) // 메인 스플리터 저장 추가
    {
        MainVSplitter->SaveConfig(Config, "MainSplitterV.SplitRatio");
    }
    if (EditorHSplitter)
    {
        EditorHSplitter->SaveConfig(Config, "EditorSplitterH.SplitRatio");
    }
    if (ViewportVSplitter)
    {
        ViewportVSplitter->SaveConfig(Config, "ViewportSplitterV.SplitRatio");
    }
    if (ViewportHSplitter)
    {
        ViewportHSplitter->SaveConfig(Config, "ViewportSplitterH.SplitRatio");
    }

    for (const auto& ViewportClient : ViewportClients)
    {
        ViewportClient->SaveConfig(Config);
    }
    ActiveViewportClient->SaveConfig(Config);

    RECT WndRect = {};
    GetWindowRect(GEngineLoop.MainAppWnd, &WndRect);
    Config["WindowX"] = std::to_string(WndRect.left);
    Config["WindowY"] = std::to_string(WndRect.top);
    Config["WindowWidth"] = std::to_string(WndRect.right - WndRect.left);
    Config["WindowHeight"] = std::to_string(WndRect.bottom - WndRect.top);
    Config["Zoomed"] = std::to_string(IsZoomed(GEngineLoop.MainAppWnd));

    Config["bMultiView"] = std::to_string(bMultiViewportMode);
    Config["ActiveViewportIndex"] = std::to_string(ActiveViewportClient->ViewportIndex);
    Config["ScreenWidth"] = std::to_string(EditorWidth);
    Config["ScreenHeight"] = std::to_string(EditorHeight);
    Config["OrthoPivotX"] = std::to_string(ActiveViewportClient->Pivot.X);
    Config["OrthoPivotY"] = std::to_string(ActiveViewportClient->Pivot.Y);
    Config["OrthoPivotZ"] = std::to_string(ActiveViewportClient->Pivot.Z);
    Config["OrthoZoomSize"] = std::to_string(ActiveViewportClient->OrthoSize);
    WriteIniFile(IniFilePath, Config);
}

TMap<FString, FString> SLevelEditor::ReadIniFile(const FString& FilePath)
{
    TMap<FString, FString> config;
    std::ifstream file(*FilePath);
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '[' || line[0] == ';')
        {
            continue;
        }
        std::istringstream ss(line);
        std::string key, value;
        if (std::getline(ss, key, '=') && std::getline(ss, value))
        {
            config[key] = value;
        }
    }
    return config;
}

void SLevelEditor::WriteIniFile(const FString& FilePath, const TMap<FString, FString>& Config)
{
    std::ofstream file(*FilePath);
    for (const auto& pair : Config)
    {
        file << *pair.Key << "=" << *pair.Value << "\n";
    }
}

void SLevelEditor::RegisterEditorInputDelegates()
{
    FSlateAppMessageHandler* Handler = GEngineLoop.GetAppMessageHandler();

    // Clear current delegate functions
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

    InputDelegatesHandles.Add(Handler->OnMouseDownDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
        {
            if (ImGui::GetIO().WantCaptureMouse)
            {
                return;
            }

            switch (InMouseEvent.GetEffectingButton())  // NOLINT(clang-diagnostic-switch-enum)
            {
            case EKeys::LeftMouseButton:
            {
                if (const UEditorEngine* EdEngine = Cast<UEditorEngine>(GEngine))
                {
                    if (const AActor* SelectedActor = EdEngine->GetSelectedActor())
                    {
                        USceneComponent* TargetComponent = nullptr;
                        if (USceneComponent* SelectedComponent = EdEngine->GetSelectedComponent())
                        {
                            TargetComponent = SelectedComponent;
                        }
                        else if (AActor* SelectedActor = EdEngine->GetSelectedActor())
                        {
                            TargetComponent = SelectedActor->GetRootComponent();
                        }
                        else
                        {
                            return;
                        }

                        // 초기 Actor와 Cursor의 거리차를 저장
                        const FViewportCamera* ViewTransform = ActiveViewportClient->GetViewportType() == LVT_Perspective
                            ? &ActiveViewportClient->PerspectiveCamera
                            : &ActiveViewportClient->OrthogonalCamera;

                        FVector RayOrigin, RayDir;
                        ActiveViewportClient->DeprojectFVector2D(FWindowsCursor::GetClientPosition(), RayOrigin, RayDir);

                        const FVector TargetLocation = TargetComponent->GetWorldLocation();
                        const float TargetDist = FVector::Distance(ViewTransform->GetLocation(), TargetLocation);
                        const FVector TargetRayEnd = RayOrigin + RayDir * TargetDist;
                        TargetDiff = TargetLocation - TargetRayEnd;
                    }
                }
                break;
            }
            case EKeys::RightMouseButton:
            {
                if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
                {
                    FWindowsCursor::SetShowMouseCursor(false);
                    MousePinPosition = InMouseEvent.GetScreenSpacePosition();
                }
                break;
            }
            default:
                break;
            }

            POINT Point;
            GetCursorPos(&Point);
            ScreenToClient(GEngineLoop.MainAppWnd, &Point);
            FVector2D ClientPos = FVector2D{ static_cast<float>(Point.x), static_cast<float>(Point.y) };

            MainVSplitter->OnPressed({ ClientPos.X, ClientPos.Y });
            EditorHSplitter->OnPressed({ ClientPos.X, ClientPos.Y });

            if (bMultiViewportMode)
            {
                SelectViewport(ClientPos);
                ViewportHSplitter->OnPressed({ ClientPos.X, ClientPos.Y });
                ViewportVSplitter->OnPressed({ ClientPos.X, ClientPos.Y });
            }
        }));

    InputDelegatesHandles.Add(Handler->OnMouseMoveDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
        {
            if (ImGui::GetIO().WantCaptureMouse) return;

            // Splitter 움직임 로직
            if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
            {
                const auto& [DeltaX, DeltaY] = InMouseEvent.GetCursorDelta();

                bool bIsSplitterDragging = false;
                if (MainVSplitter && MainVSplitter->IsSplitterPressed())
                {
                    MainVSplitter->OnDrag(FPoint(DeltaX, DeltaY));
                    bIsSplitterDragging = true;
                }
                if (EditorHSplitter && EditorHSplitter->IsSplitterPressed())
                {
                    EditorHSplitter->OnDrag(FPoint(DeltaX, DeltaY));
                    bIsSplitterDragging = true;
                }
                if (ViewportHSplitter && ViewportHSplitter->IsSplitterPressed())
                {
                    ViewportHSplitter->OnDrag(FPoint(DeltaX, DeltaY));
                    bIsSplitterDragging = true;
                }
                if (ViewportVSplitter && ViewportVSplitter->IsSplitterPressed())
                {
                    ViewportVSplitter->OnDrag(FPoint(DeltaX, DeltaY));
                    bIsSplitterDragging = true;
                }

                if (bIsSplitterDragging)
                {
                    // Explicitly resize child splitters based on the new MainVSplitter areas
                    if (MainVSplitter)
                    {
                        FRect ViewportAreaRect = MainVSplitter->SideLT->GetRect();
                        if (ViewportVSplitter && ViewportHSplitter)
                        {
                            ViewportVSplitter->SetRect(ViewportAreaRect);
                            ViewportHSplitter->SetRect(ViewportAreaRect);
                        }

                        FRect PanelAreaRect = MainVSplitter->SideRB->GetRect();
                        if (EditorHSplitter)
                        {
                            EditorHSplitter->SetRect(PanelAreaRect);
                        }
                    }

                    ResizeViewports();
                }
            }

            // 커서 변경 로직
            if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && !InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
            {
                // TODO: 나중에 커서가 MainViewport 위에 있을때만 ECursorType::Crosshair로 바꾸게끔 하기 (Viewer의 경우 Arrow임)
                // ECursorType CursorType = ECursorType::Crosshair;
                ECursorType CursorType = ECursorType::Arrow;
                POINT Point;

                GetCursorPos(&Point);
                ScreenToClient(GEngineLoop.MainAppWnd, &Point);
                FVector2D ClientPos = FVector2D{ static_cast<float>(Point.x), static_cast<float>(Point.y) };

                // 모든 스플리터에 대해 Hover 검사
                bool bMainVHovered = MainVSplitter->IsSplitterHovered({ ClientPos.X, ClientPos.Y });
                bool bEditorHHovered = EditorHSplitter->IsSplitterHovered({ ClientPos.X, ClientPos.Y });
                if (bMainVHovered)
                {
                    CursorType = ECursorType::ResizeLeftRight;
                }
                else if (bEditorHHovered)
                {
                    CursorType = ECursorType::ResizeUpDown;
                }
                else if (bMultiViewportMode)
                {
                    bool bViewportVHovered = ViewportHSplitter->IsSplitterHovered({ ClientPos.X, ClientPos.Y });
                    bool bViewportHHovered = ViewportVSplitter->IsSplitterHovered({ ClientPos.X, ClientPos.Y });
                    if (bViewportHHovered && bViewportVHovered)
                    {
                        CursorType = ECursorType::ResizeAll;
                    }
                    else if (bViewportHHovered)
                    {
                        CursorType = ECursorType::ResizeLeftRight;
                    }
                    else if (bViewportVHovered)
                    {
                        CursorType = ECursorType::ResizeUpDown;
                    }
                }

                FWindowsCursor::SetMouseCursor(CursorType);
            }
        }));

    InputDelegatesHandles.Add(Handler->OnMouseUpDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
        {
            switch (InMouseEvent.GetEffectingButton())  // NOLINT(clang-diagnostic-switch-enum)
            {
            case EKeys::RightMouseButton:
                {
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
                    if (MainVSplitter)
                    {
                        MainVSplitter->OnReleased();
                    }
                    if (EditorHSplitter)
                    {
                        EditorHSplitter->OnReleased();
                    }
                    if (ViewportHSplitter)
                    {
                        ViewportHSplitter->OnReleased();
                    }
                    if (ViewportVSplitter)
                    {
                        ViewportVSplitter->OnReleased();
                    }
                    return;
                }

            default:
                return;
            }
        }));

    InputDelegatesHandles.Add(Handler->OnRawMouseInputDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
        {
            // Mouse Move 이벤트 일때만 실행
            if (InMouseEvent.GetInputEvent() == IE_Axis
             && InMouseEvent.GetEffectingButton() == EKeys::Invalid)
            {
                // 에디터 카메라 이동 로직
                if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton)
                  && InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
                {
                    ActiveViewportClient->MouseMove(InMouseEvent);
                }
                // Gizmo control
                else if (!InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton)
                       && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
                {
                    if (const UEditorEngine* EdEngine = Cast<UEditorEngine>(GEngine))
                    {
                        const UGizmoBaseComponent* Gizmo = Cast<UGizmoBaseComponent>(ActiveViewportClient->GetPickedGizmoComponent());
                        if (!Gizmo)
                        {
                            return;
                        }

                        USceneComponent* TargetComponent = EdEngine->GetSelectedComponent();
                        if (!TargetComponent)
                        {
                            if (AActor* SelectedActor = EdEngine->GetSelectedActor())
                            {
                                TargetComponent = SelectedActor->GetRootComponent();
                            }
                            else
                            {
                                return;
                            }
                        }

                        const FViewportCamera* ViewTransform = ActiveViewportClient->GetViewportType() == LVT_Perspective
                            ? &ActiveViewportClient->PerspectiveCamera
                            : &ActiveViewportClient->OrthogonalCamera;

                        FVector RayOrigin, RayDir;
                        ActiveViewportClient->DeprojectFVector2D(FWindowsCursor::GetClientPosition(), RayOrigin, RayDir);

                        const float TargetDist = FVector::Distance(ViewTransform->GetLocation(), TargetComponent->GetWorldLocation());
                        const FVector TargetRayEnd = RayOrigin + RayDir * TargetDist;
                        const FVector Result = TargetRayEnd + TargetDiff;

                        FVector NewLocation = TargetComponent->GetWorldLocation();
                        if (EdEngine->GetEditorPlayer()->GetCoordMode() == CDM_WORLD)
                        {
                            // 월드 좌표계에서 카메라 방향을 고려한 이동
                            if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowX)
                            {
                                // 카메라의 오른쪽 방향을 X축 이동에 사용
                                NewLocation.X = Result.X;
                            }
                            else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowY)
                            {
                                // 카메라의 오른쪽 방향을 Y축 이동에 사용
                                NewLocation.Y = Result.Y;
                            }
                            else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowZ)
                            {
                                // 카메라의 위쪽 방향을 Z축 이동에 사용
                                NewLocation.Z = Result.Z;
                            }
                        }
                        else
                        {
                            // Result에서 현재 액터 위치를 빼서 이동 벡터를 구함
                            const FVector Delta = Result - TargetComponent->GetWorldLocation();
                            // 각 축에 대해 Local 방향 벡터에 투영하여 이동량 계산
                            if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowX)
                            {
                                const float MoveAmount = FVector::DotProduct(Delta, TargetComponent->GetForwardVector());
                                NewLocation += TargetComponent->GetForwardVector() * MoveAmount;
                            }
                            else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowY)
                            {
                                const float MoveAmount = FVector::DotProduct(Delta, TargetComponent->GetRightVector());
                                NewLocation += TargetComponent->GetRightVector() * MoveAmount;
                                TargetComponent->SetWorldLocation(NewLocation);
                            }
                            else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowZ)
                            {
                                const float MoveAmount = FVector::DotProduct(Delta, TargetComponent->GetUpVector());
                                NewLocation += TargetComponent->GetUpVector() * MoveAmount;
                            }
                        }
                        TargetComponent->SetWorldLocation(NewLocation);
                    }
                }
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

    InputDelegatesHandles.Add(Handler->OnMouseWheelDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
        {
            if (ImGui::GetIO().WantCaptureMouse)
            {
                return;
            }

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
        }));

    InputDelegatesHandles.Add(Handler->OnKeyDownDelegate.AddLambda([this](const FKeyEvent& InKeyEvent)
        {
            ActiveViewportClient->InputKey(InKeyEvent);
        }));

    InputDelegatesHandles.Add(Handler->OnKeyUpDelegate.AddLambda([this](const FKeyEvent& InKeyEvent)
        {
            ActiveViewportClient->InputKey(InKeyEvent);
        }));
}

void SLevelEditor::RegisterPIEInputDelegates()
{
    FSlateAppMessageHandler* Handler = GEngineLoop.GetAppMessageHandler();

    // Clear current delegate functions
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
    // Add Delegate functions in PIE mode
}
