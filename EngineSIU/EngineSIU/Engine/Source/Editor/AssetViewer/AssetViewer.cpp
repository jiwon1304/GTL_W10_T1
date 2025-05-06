#include "AssetViewer.h"

#include "UObject/Object.h"
#include "EngineLoop.h" // Assuming Launch module, adjust if needed
#include "UnrealClient.h"
#include "Widgets/SWindow.h" // Assuming SlateCore module
#include "Widgets/Layout/SSplitter.h"
#include "UnrealEd/EditorViewportClient.h"

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
    EditorWidth = InEditorWidth;
    EditorHeight = InEditorHeight;

    if (PrimaryVSplitter)
    {
        PrimaryVSplitter->SetRect(FRect(0, 0, static_cast<float>(InEditorWidth), static_cast<float>(InEditorHeight)));
        PrimaryVSplitter->OnResize(InEditorWidth, InEditorHeight);
    }

    ResizeViewport();
}

void SAssetViewer::ResizeViewport()
{
    if (PrimaryVSplitter)
    {
        PrimaryVSplitter->UpdateChildRects();
    }

    if (ActiveViewportClient && ActiveViewportClient->GetViewport() && CenterAndRightVSplitter && CenterAndRightVSplitter->SideLT)
    {
        ActiveViewportClient->GetViewport()->ResizeViewport(CenterAndRightVSplitter->SideLT->GetRect());
    }
}

void SAssetViewer::SelectViewport(const FVector2D& Point)
{
    if (ActiveViewportClient && CenterAndRightVSplitter && CenterAndRightVSplitter->SideLT)
    {
        if (ActiveViewportClient->IsSelected(Point))
        {
            SetFocus(GEngineLoop.SkeletalMeshViewerAppWnd);
        }
    }
}

void SAssetViewer::RegisterViewerInputDelegates()
{
    //FSlateAppMessageHandler* Handler = GEngineLoop.GetAppMessageHandler();


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
