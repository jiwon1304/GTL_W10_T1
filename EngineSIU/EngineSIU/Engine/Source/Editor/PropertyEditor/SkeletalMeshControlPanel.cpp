#include "SkeletalMeshControlPanel.h"
#include "Engine/EditorEngine.h"
#include "UnrealEd/EditorViewportClient.h"
#include "EngineLoop.h"
#include "Viewer/SlateViewer.h"
#include "tinyfiledialogs.h"
#include "WindowsFileDialog.h"
#include "Engine/AssetManager.h"

void SkeletalMeshControlPanel::Render()
{
    /* Pre Setup */
    const ImGuiIO& IO = ImGui::GetIO();
    ImFont* IconFont = IO.Fonts->Fonts.size() == 1 ? IO.FontDefault : IO.Fonts->Fonts[FEATHER_FONT];
    constexpr ImVec2 IconSize = ImVec2(32, 32);

    const float PanelWidth = (Width) * 0.8f;
    constexpr float PanelHeight = 72.f;

    constexpr float PanelPosX = 0.0f;
    constexpr float PanelPosY = 0.0f;

    constexpr ImVec2 MinSize(300, 72);
    constexpr ImVec2 MaxSize(FLT_MAX, 72);

    /* Min, Max Size */
    ImGui::SetNextWindowSizeConstraints(MinSize, MaxSize);

    /* Panel Position */
    ImGui::SetNextWindowPos(ImVec2(PanelPosX, PanelPosY), ImGuiCond_Always);

    /* Panel Size */
    ImGui::SetNextWindowSize(ImVec2(PanelWidth, PanelHeight), ImGuiCond_Always);

    /* Panel Flags */
    constexpr ImGuiWindowFlags PanelFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_MenuBar;

    /* Render Start */
    if (!ImGui::Begin("Skeletal Mesh Control Panel", nullptr, PanelFlags))
    {
        ImGui::End();
        return;
    }

    /* Menu Bar */
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            CreateSkeletalMenuButton(IconSize, IconFont);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Show Bones"))
            {
                bShowBoneOptions = !bShowBoneOptions;
            }
            
            if (ImGui::BeginMenu("Display Mode"))
            {
                auto ViewportClient = GEngineLoop.GetSkeletalMeshViewer()->GetActiveViewportClient();
                
                const char* ViewModeNames[] = { 
                    "Lit_Gouraud", "Lit_Lambert", "Lit_Blinn-Phong", "Lit_PBR",
                    "Unlit", "Wireframe",
                    "Scene Depth", "World Normal", "World Tangent","Light Heat Map"
                };
                
                for (int i = 0; i < IM_ARRAYSIZE(ViewModeNames); i++)
                {
                    bool bIsSelected = (static_cast<int>(ViewportClient->GetViewMode()) == i);
                    if (ImGui::MenuItem(ViewModeNames[i], nullptr, bIsSelected))
                    {
                        ViewportClient->SetViewMode(static_cast<EViewModeIndex>(i));
                    }
                }
                
                ImGui::EndMenu();
            }
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Animation"))
        {
            if (ImGui::MenuItem("Animation Controls"))
            {
                bShowAnimationControls = !bShowAnimationControls;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // 스켈레탈 메시 컨트롤 추가
    CreateSkeletalMeshControls(IconSize);
    ImGui::SameLine();
    
    // 애니메이션 컨트롤 버튼 추가
    CreateAnimationControls(IconSize, IconFont);
    
    // 본 옵션 팝업 처리
    if (bShowBoneOptions)
    {
        CreateBoneDisplayOptions();
    }
    
    // 애니메이션 컨트롤 팝업
    if (bShowAnimationControls)
    {
        ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Animation Controls", &bShowAnimationControls))
        {
            // 애니메이션 재생/정지 버튼
            if (ImGui::Button("Play"))
            {
                // 애니메이션 재생 코드
            }
            ImGui::SameLine();
            if (ImGui::Button("Pause"))
            {
                // 애니메이션 일시 정지 코드
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
            {
                // 애니메이션 정지 코드
            }
            
            // 애니메이션 프레임 스크러빙
            static float currentFrame = 0.0f;
            static float maxFrames = 100.0f;
            ImGui::SliderFloat("Frame", &currentFrame, 0.0f, maxFrames, "%.0f");
            
            // 재생 속도 조절
            static float playbackSpeed = 1.0f;
            ImGui::SliderFloat("Speed", &playbackSpeed, 0.1f, 2.0f, "%.1f");
            
            ImGui::Separator();
            
            // 애니메이션 목록 (예시)
            static int selectedAnimation = 0;
            const char* animations[] = { "None", "Idle", "Walk", "Run", "Jump", "Attack" };
            ImGui::Combo("Animation", &selectedAnimation, animations, IM_ARRAYSIZE(animations));
        }
        ImGui::End();
    }

    ImGui::End();
}

void SkeletalMeshControlPanel::OnResize(HWND hWnd)
{
    if (hWnd != Handle)
    {
        return;
    }
    
    RECT ClientRect;
    GetClientRect(hWnd, &ClientRect);
    Width = static_cast<FLOAT>(ClientRect.right - ClientRect.left);
    Height = static_cast<FLOAT>(ClientRect.bottom - ClientRect.top);
}

void SkeletalMeshControlPanel::CreateSkeletalMenuButton(const ImVec2 ButtonSize, ImFont* IconFont)
{
    if (ImGui::MenuItem("FBX (.fbx)"))
    {
        TArray<FString> FileNames;
        if (FDesktopPlatformWindows::OpenFileDialog(
            "Open FBX File",
            "",
            { {
                .FilterPattern = "*.fbx",
                .Description = "FBX(.fbx) file"
            } },
            EFileDialogFlag::None,
            FileNames
        ))
        {
            const FString FileName = FileNames.Pop();
            UE_LOG(ELogLevel::Display, TEXT("Import FBX File: %s"), *FileName);



            if (UAssetManager::Get().AddAsset(StringToWString(GetData(FileName))) == false)
            {
                tinyfd_messageBox("Error", "파일을 불러올 수 없습니다.", "ok", "error", 1);
            }

        }
    }

    if (ImGui::MenuItem("Import Animation"))
    {
        char const* lFilterPatterns[1] = { "*.fbx" };
        const char* FileName = tinyfd_openFileDialog("Open Animation File", "", 1, lFilterPatterns, "Animation Files (*.fbx)", 0);

        if (FileName == nullptr)
        {
            return;
        }
        
        UE_LOG(ELogLevel::Display, TEXT("Animation File: %s"), *FString(FileName));
        
        // 애니메이션 임포트 코드 구현
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Save Skeletal Mesh"))
    {
        char const* lFilterPatterns[1] = { "*.fbx" };
        const char* FileName = tinyfd_saveFileDialog("Save Skeletal Mesh File", "", 1, lFilterPatterns, "Skeletal Mesh Files (*.fbx)");

        if (FileName == nullptr)
        {
            return;
        }
        
        UE_LOG(ELogLevel::Display, TEXT("Save Skeletal Mesh to: %s"), *FString(FileName));
        // 저장 코드 구현
    }
}

void SkeletalMeshControlPanel::CreateSkeletalMeshControls(const ImVec2 ButtonSize)
{
    // 본 표시 버튼
    if (ImGui::Button("Bone Display", ImVec2(100, ButtonSize.y)))
    {
        bShowBoneOptions = !bShowBoneOptions;
    }
    
    ImGui::SameLine();
    
    // 카메라 리셋 버튼
    if (ImGui::Button("Reset Camera", ImVec2(100, ButtonSize.y)))
    {
        auto ViewportClient = GEngineLoop.GetSkeletalMeshViewer()->GetActiveViewportClient();
        if (ViewportClient)
        {
            // ResetCamera 메서드가 없으므로 카메라 위치를 직접 설정
            ViewportClient->PerspectiveCamera.SetLocation(FVector(0, 100, 0));
            ViewportClient->PerspectiveCamera.SetRotation(FVector(0, 0, 0));
        }
    }
}

void SkeletalMeshControlPanel::CreateAnimationControls(const ImVec2 ButtonSize, ImFont* IconFont)
{
    // 애니메이션 컨트롤 버튼
    if (ImGui::Button("Animation", ImVec2(100, ButtonSize.y)))
    {
        bShowAnimationControls = !bShowAnimationControls;
    }
}

void SkeletalMeshControlPanel::CreateBoneDisplayOptions()
{
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Bone Display Options", &bShowBoneOptions))
    {
        ImGui::Checkbox("Show All Bones", nullptr);
        ImGui::Checkbox("Show Names", nullptr);
        ImGui::Checkbox("Show Axes", nullptr);
        
        ImGui::Separator();
        
        // 본 크기 조절
        static float boneSize = 1.0f;
        ImGui::SliderFloat("Bone Size", &boneSize, 0.1f, 3.0f, "%.1f");
        
        // 본 색상 조절
        static float boneColor[3] = { 1.0f, 0.0f, 0.0f }; // 빨간색 기본값
        ImGui::ColorEdit3("Bone Color", boneColor);
    }
    ImGui::End();
}
