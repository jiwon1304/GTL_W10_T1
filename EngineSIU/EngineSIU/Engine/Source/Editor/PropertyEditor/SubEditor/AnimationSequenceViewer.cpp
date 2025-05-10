#include "AnimationSequenceViewer.h"
#include "ImGui/imgui_neo_sequencer.h"

// UI Sample
//https://dev.epicgames.com/documentation/ko-kr/unreal-engine#%EC%95%A0%EB%8B%88%EB%A9%94%EC%9D%B4%EC%85%98%EC%8B%9C%ED%80%80%EC%8A%A4%ED%8E%B8%EC%A7%91%ED%95%98%EA%B8%B0

void AnimationSequenceViewer::Render()
{
    ImVec2 WinSize = ImVec2(Width, Height);

    /* Flags */
    ImGuiWindowFlags PanelFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar;

    /* Animation Sequencer */
    ImGui::SetNextWindowPos(ImVec2(0, WinSize.y * 0.7f));
    ImGui::SetNextWindowSize(ImVec2(WinSize.x * 0.8f - 5.0f, WinSize.y * 0.3f));
    
    ImGui::Begin("Animation Sequencer", nullptr, PanelFlags);

    RenderPlayController(ImGui::GetContentRegionAvail().x, 32.f);
    RenderAnimationSequence(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    
    ImGui::End();

    /* Asset Details */
    ImGui::SetNextWindowPos(ImVec2(WinSize.x * 0.8f, 0));
    ImGui::SetNextWindowSize(ImVec2(WinSize.x * 0.2f, WinSize.y * 0.7f - 5.0f));

    ImGui::Begin("Asset Details", nullptr, PanelFlags);

    RenderAssetDetails();
    
    ImGui::End();


    /* Asset Browser */
    ImGui::SetNextWindowPos(ImVec2(WinSize.x * 0.8f, WinSize.y * 0.7f));
    ImGui::SetNextWindowSize(ImVec2(WinSize.x * 0.2f, WinSize.y * 0.3f));
    
    ImGui::Begin("Asset Browser", nullptr, PanelFlags);

    RenderAssetBrowser();
    
    ImGui::End();
}

void AnimationSequenceViewer::OnResize(HWND hWnd)
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

void AnimationSequenceViewer::RenderAnimationSequence(float Width, float Height) const
{
    static int32_t currentFrame = 0;
    static int32_t startFrame = 0;
    static int32_t endFrame = 64;
    static bool transformOpen = false;
    std::vector<ImGui::FrameIndexType> keys = {0, 10, 24};
    bool doDelete = false;

    if (ImGui::BeginNeoSequencer("Sequencer", &currentFrame, &startFrame, &endFrame, {Width, Height},
                                 ImGuiNeoSequencerFlags_EnableSelection |
                                 ImGuiNeoSequencerFlags_AllowLengthChanging |
                                 ImGuiNeoSequencerFlags_Selection_EnableDeletion))
    {
        if (ImGui::BeginNeoGroup("Notifies", &transformOpen))
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                UE_LOG(ELogLevel::Warning, "RIGHT CLICKED");
            }
            
            if (ImGui::BeginNeoTimelineEx("1"))
            {
                for (auto&& v: keys)
                {
                    ImGui::NeoKeyframe(&v);
                    // Per keyframe code here
                }
                
                ImGui::EndNeoTimeLine();
            }
            ImGui::EndNeoGroup();
        }

        ImGui::EndNeoSequencer();
    }
}

void AnimationSequenceViewer::RenderPlayController(float Width, float Height) const
{
    const ImGuiIO& IO = ImGui::GetIO();
    ImFont* IconFont = IO.Fonts->Fonts.size() == 1 ? IO.FontDefault : IO.Fonts->Fonts[FEATHER_FONT];
    constexpr ImVec2 IconSize = ImVec2(32, 32);
    
    ImGui::BeginChild("PlayController", ImVec2(Width, Height), ImGuiChildFlags_AutoResizeX, ImGuiWindowFlags_NoMove);
    ImGui::PushFont(IconFont);

    if (ImGui::Button("\ue9d2", IconSize)) // Rewind
    {
        
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("\ue9a8", IconSize)) // Play & Stop
    {
        
    }

    ImGui::SameLine();

    if (ImGui::Button("\ue96a", IconSize)) // Fast-forward
    {
        
    }

    ImGui::SameLine();

    if (ImGui::Button("\ue9d1", IconSize)) // Repeat
    {
        
    }
    ImGui::PopFont();

    ImGui::SameLine();

    ImGui::Text("Ani Percentage: %.2lf%% CurrentTime: %.3lf/%.3lf (second(s)) CurrentFrame: %.2lf/ %dFrame", 0.1f, 0.1f, 0.1f, 0.1f, 1);
    
    ImGui::EndChild();
}

void AnimationSequenceViewer::RenderAssetDetails() const
{
    
}

void AnimationSequenceViewer::RenderAssetBrowser() const
{
    
}
