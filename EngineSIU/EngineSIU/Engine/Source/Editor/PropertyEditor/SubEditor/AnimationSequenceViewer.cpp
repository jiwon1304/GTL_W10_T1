#include "AnimationSequenceViewer.h"

#include "Components/Mesh/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "Contents/Actors/ItemActor.h"
#include "Engine/EditorEngine.h"
#include "Engine/FFbxLoader.h"

// UI Sample
//https://dev.epicgames.com/documentation/ko-kr/unreal-engine#%EC%95%A0%EB%8B%88%EB%A9%94%EC%9D%B4%EC%85%98%EC%8B%9C%ED%80%80%EC%8A%A4%ED%8E%B8%EC%A7%91%ED%95%98%EA%B8%B0

void AnimationSequenceViewer::Render()
{
    ImVec2 WinSize = ImVec2(Width, Height);

    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    if (Engine)
    {
        for (auto Actor : Engine->GetPreviewWorld(GEngineLoop.AnimationViewerAppWnd)->GetActiveLevel()->Actors)
        {
            if (Actor && Actor->IsA<AItemActor>())
            {
                SelectedSkeletalMeshComponent = Cast<AItemActor>(Actor)->GetComponentByClass<USkeletalMeshComponent>();
            }
        }
    }

    // Update CurrentFrameSeconds if playing
    if (bIsPlaying && SelectedAnimSequence && SelectedAnimSequence->GetDataModel())
    {
        float deltaTime = ImGui::GetIO().DeltaTime;
        CurrentFrameSeconds += deltaTime;
        
        if (CurrentFrameSeconds >= MaxFrameSeconds)
        {
            if (bIsRepeating)
            {
                CurrentFrameSeconds = fmodf(CurrentFrameSeconds, MaxFrameSeconds);
                if (SelectedSkeletalMeshComponent)
                {
                    SelectedSkeletalMeshComponent->PlayAnimation(SelectedAnimSequence, bIsRepeating);
                }
            }
            else
            {
                CurrentFrameSeconds = MaxFrameSeconds;
                bIsPlaying = false; // Stop playing
            }
        }
    }

    
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

void AnimationSequenceViewer::RenderAnimationSequence(float InWidth, float InHeight)
{
    if (SelectedAnimSequence == nullptr || SelectedAnimSequence->GetDataModel() == nullptr)
    {
        return;
    }
    
    static bool transformOpen = false;
    TArray<FAnimNotifyEvent>& Notifies =  SelectedAnimSequence->Notifies;

    if (SelectedAnimSequence && SelectedAnimSequence->GetDataModel() && bNeedsNotifyUpdate)
    {
        TrackAndKeyMap.Empty();
        for (auto& v : Notifies)
        {
            TrackAndKeyMap[v.TrackIndex].Add(v.TriggerTime);
        }
        bNeedsNotifyUpdate = false;
    }
    
    if (ImGui::BeginNeoSequencer("Sequencer", &CurrentFrameSeconds, &StartFrameSeconds, &EndFrameSeconds, {InWidth, InHeight},
                                 ImGuiNeoSequencerFlags_EnableSelection |
                                 ImGuiNeoSequencerFlags_AllowLengthChanging |
                                 ImGuiNeoSequencerFlags_Selection_EnableDeletion))
    {
        EndFrameSeconds = std::min(EndFrameSeconds, MaxFrameSeconds);

        if (ImGui::BeginNeoGroup("Notifies", &transformOpen))
        {
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                UE_LOG(ELogLevel::Warning, "RIGHT CLICKED");
                ImGui::OpenPopup("TrackPopup");
            }
            
            if (ImGui::BeginPopup("TrackPopup"))
            {
                if (ImGui::BeginMenu("Track"))
                {
                    if (ImGui::MenuItem("Add Track"))
                    {
                        int FindTrackIndex = FindAvailableTrackIndex(Notifies);
                        TrackAndKeyMap.Add(FindTrackIndex, TArray<ImGui::FrameIndexType>());
                        TrackList.Add(FindTrackIndex);
                        bNeedsNotifyUpdate = true;
                    }
                    
                    if (ImGui::MenuItem("Remove Track"))
                    {
                        if (SelectedTrackIndex >= 0)
                        {
                            TrackList.Remove(SelectedTrackIndex);
                            
                            for (int32 i = Notifies.Num() - 1; i >= 0; --i)
                            {
                                if (Notifies[i].TrackIndex == SelectedTrackIndex)
                                {
                                    Notifies.RemoveAt(i);
                                }
                            }
                            
                            SelectedTrackIndex = -1;
                            SelectedNotifyIndex = -1;
                            bNeedsNotifyUpdate = true;
                        }
                    }
                    
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }

            for (int32 TrackIndex : TrackList)
            {
                FString TimelineId = FString::Printf(TEXT("Track_%d"), TrackIndex);
                
                if (ImGui::BeginNeoTimelineEx(GetData(TimelineId)))
                {
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    {
                        SelectedTrackIndex = TrackIndex;
                        ImGui::OpenPopup("NotifyPopup_Generic"); // 우선은 하나의 제네릭 팝업 사용
                    }
                
                    // 해당 트랙에 속한 Notify만 찾아서 렌더링
                    for (int32 i = 0; i < Notifies.Num(); ++i)
                    {
                        if (Notifies[i].TrackIndex == TrackIndex)
                        {
                            ImGui::FrameIndexType& TimeRef = Notifies[i].TriggerTime;
                            ImGui::NeoKeyframe(&TimeRef);

                            if (ImGui::IsNeoKeyframeSelected())
                            {
                                SelectedTrackIndex = TrackIndex;
                                SelectedNotifyIndex = i;
                            }

                            if (ImGui::IsNeoKeyframeRightClicked())
                            {
                                ImGui::OpenPopup("KeyframePopup");
                            }
                        }
                    }

                    if (ImGui::BeginPopup("KeyframePopup"))
                    {
                        if (ImGui::MenuItem("Delete Notify"))
                        {
                            if (SelectedNotifyIndex >= 0 && SelectedNotifyIndex < Notifies.Num())
                            {
                                Notifies.RemoveAt(SelectedNotifyIndex);
                                SelectedNotifyIndex = -1;
                                bNeedsNotifyUpdate = true;
                            }
                        }
                                
                        ImGui::EndPopup();
                    }
                    
                    ImGui::SetItemDefaultFocus();
                    
                    ImGui::EndNeoTimeLine();
                }
            }
            
            // 모든 타임라인에 공통으로 적용될 수 있는 팝업 (혹은 위에서처럼 각 타임라인별 팝업)
            if (ImGui::BeginPopup("NotifyPopup_Generic"))
            {
                if (ImGui::BeginMenu("Notify"))
                {
                    if (ImGui::MenuItem("Add Notify"))
                    {
                        UE_LOG(ELogLevel::Display, "Add Notify Clicked");
                        
                        FAnimNotifyEvent NewNotify;
                        NewNotify.TrackIndex = SelectedTrackIndex;
                        NewNotify.TriggerTime = CurrentFrameSeconds;
                        NewNotify.Duration = 0.0f;
                        NewNotify.NotifyName = "NONE";
                        
                        Notifies.Add(NewNotify);
                        bNeedsNotifyUpdate = true;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }
            
            ImGui::EndNeoGroup();
        }

        if ( SelectedSkeletalMeshComponent->GetSingleNodeInstance() != nullptr)
        {
             SelectedSkeletalMeshComponent->GetSingleNodeInstance()->SetCurrentTime(CurrentFrameSeconds);
        }
        
        ImGui::EndNeoSequencer();
    }
}

void AnimationSequenceViewer::RenderPlayController(float InWidth, float InHeight)
{
    const ImGuiIO& IO = ImGui::GetIO();
    ImFont* IconFont = IO.Fonts->Fonts.size() == 1 ? IO.FontDefault : IO.Fonts->Fonts[FEATHER_FONT];
    constexpr ImVec2 IconSize = ImVec2(32, 32);
    UAnimSingleNodeInstance* SingleNode =  SelectedSkeletalMeshComponent->GetSingleNodeInstance();
    
    ImGui::BeginChild("PlayController", ImVec2(InWidth, InHeight), ImGuiChildFlags_AutoResizeX, ImGuiWindowFlags_NoMove);
    ImGui::PushFont(IconFont);

    if (ImGui::Button("\ue9d2", IconSize)) // Rewind
    {
        CurrentFrameSeconds = 0.0f;
        bIsPlaying = false;
    }
    
    ImGui::SameLine();

    const char* PlayIcon = bIsPlaying ? "\ue99c" : "\ue9a8";
    if (ImGui::Button(PlayIcon, IconSize)) // Play & Stop
    {
        if (SelectedAnimIndex == -1 || SelectedAnimName.IsEmpty() || SelectedAnimSequence == nullptr)
        {
            ImGui::PopFont();
            ImGui::EndChild();
            return;
            
        }
        
        bIsPlaying = !bIsPlaying;
        
        if (bIsPlaying)
        {
            // Already Playing
            if (CurrentFrameSeconds < MaxFrameSeconds && CurrentFrameSeconds > 0.0f)
            {
                SingleNode->GetCurrentSequence()->SetRateScale(1.0f);
            }
            else 
            {
                // Play
                if (CurrentFrameSeconds >= MaxFrameSeconds && MaxFrameSeconds > 0.0f)
                {
                    CurrentFrameSeconds = 0.0f;
                }

                SelectedSkeletalMeshComponent->PlayAnimation(SelectedAnimSequence, bIsRepeating);
            }
        }
        else
        {
            // Pause
            if (SingleNode != nullptr)
            {
                SingleNode->GetCurrentSequence()->SetRateScale(0.0f);
                
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("\ue96a", IconSize)) // Fast-forward
    {
        if (SelectedAnimSequence && SelectedAnimSequence->GetDataModel())
        {
            CurrentFrameSeconds = MaxFrameSeconds;
        }
        
        if (!bIsRepeating)
        {
            bIsPlaying = false; // Stop Play
        }
    }

    ImGui::SameLine();
    
    RepeatButton(&bIsRepeating);
    
    ImGui::PopFont();

    ImGui::SameLine();

    
    // Slider and Text display
    if (SelectedAnimSequence && SelectedAnimSequence->GetDataModel())
    {
        const UAnimDataModel* dataModel = SelectedAnimSequence->GetDataModel();
        int totalFrames = FMath::Max(1, dataModel->NumberOfFrames); // 0 방지

        float frameRate = dataModel->FrameRate.AsDecimal();
        int sliderMax = totalFrames - 1;

        int currentFrameInt = static_cast<int>(floor(CurrentFrameSeconds * frameRate));
        currentFrameInt = FMath::Clamp(currentFrameInt, 0, sliderMax);

        if (ImGui::SliderInt("##FrameSlider", &currentFrameInt, 0, sliderMax))
        {
            CurrentFrameSeconds = static_cast<float>(currentFrameInt) / frameRate;
            CurrentFrameSeconds = FMath::Clamp(CurrentFrameSeconds, 0.0f, MaxFrameSeconds);
        }

        ImGui::SameLine();
    
        float animPercentage = (MaxFrameSeconds > 0.0f) ? (CurrentFrameSeconds / MaxFrameSeconds) * 100.0f : 0.0f;
        int displayCurrentFrame = currentFrameInt; 
        if(dataModel->NumberOfFrames > 0 && CurrentFrameSeconds >= MaxFrameSeconds && !bIsPlaying && !bIsRepeating) {
            displayCurrentFrame = dataModel->NumberOfFrames > 0 ? dataModel->NumberOfFrames -1 : 0; // Show last frame if at end and stopped
        }


        ImGui::Text("Time: %.2f/%.2fs | Frame: %d/%d | %.1f%%",
                    CurrentFrameSeconds,
                    MaxFrameSeconds,
                    displayCurrentFrame, // Display 0-indexed or 1-indexed based on preference, here 0-indexed
                    dataModel->NumberOfFrames > 0 ? dataModel->NumberOfFrames -1: 0, // Max frame index
                    animPercentage);
    }
    else
    {
        static int dummyFrame = 0;
        ImGui::SliderInt("##FrameSlider", &dummyFrame, 0, 100);
        ImGui::SameLine();
        ImGui::Text("Time: 0.00/0.00s | Frame: 0/0 | 0.0%%");
    }

    ImGui::EndChild();
}

void AnimationSequenceViewer::RenderAssetDetails()
{
    if (SelectedNotifyIndex == -1)
    {
        return;
    }

    FAnimNotifyEvent& Notify = SelectedAnimSequence->Notifies[SelectedNotifyIndex];
    
    static char NotifyBuffer[128] = { 0 };

    // FString → char (UTF8 변환)
    std::string NotifyNameStr = GetData(*Notify.NotifyName.ToString());
    strncpy(NotifyBuffer, NotifyNameStr.c_str(), sizeof(NotifyBuffer));
    NotifyBuffer[sizeof(NotifyBuffer) - 1] = '\0'; // null-termination 보장
    
    if (ImGui::InputText("Notify Name", NotifyBuffer, sizeof(NotifyBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        Notify.NotifyName = FName(NotifyBuffer);
        bNeedsNotifyUpdate = true;
    }
    
    float *TriggerTime = &Notify.TriggerTime;
    if (ImGui::InputFloat("TriggerTime", TriggerTime, 0.0f, 0.0f, "%.2f"))
    {
        bNeedsNotifyUpdate = true;
    }

    float* Duration = &Notify.Duration;
    if (ImGui::InputFloat("Duration", Duration, 0.0f, 0.0f, "%.2f"))
    {
        bNeedsNotifyUpdate = true;
    }
}

void AnimationSequenceViewer::RenderAssetBrowser() 
{
    TArray<FString> animNames;
    {
        std::lock_guard<std::mutex> lock(FFbxLoader::AnimMapMutex);
        for (auto const& [name, entry] : FFbxLoader::AnimMap)
        {
            if (entry.State == FFbxLoader::LoadState::Completed && entry.Sequence != nullptr)
            {
                animNames.Add(name);
            }
        }
    }
    const char* preview_value = (SelectedAnimIndex != -1 && SelectedAnimIndex < animNames.Num()) ? *animNames[SelectedAnimIndex] : "None";

    if (ImGui::BeginCombo("Animations", preview_value))
    {
        for (int i = 0; i < animNames.Num(); ++i)
        {
            const bool is_selected = (SelectedAnimIndex == i);
            if (ImGui::Selectable(*animNames[i], is_selected))
            {
                SelectedAnimIndex = i;
                SelectedAnimName = animNames[i]; 

                UAnimSequence* newSequence = FFbxLoader::GetAnimSequenceByName(SelectedAnimName);
                if (newSequence != SelectedAnimSequence) // If sequence changed
                {
                    TrackAndKeyMap.Empty();
                    SelectedTrackIndex = -1;
                    SelectedNotifyIndex = -1;
                    bNeedsNotifyUpdate = true;
                    
                    SelectedAnimSequence = newSequence;
                    bIsPlaying = false; // Stop playback for new animation
                    CurrentFrameSeconds = 0.0f; // Reset time
                    StartFrameSeconds = 0.0f;
                    if (SelectedAnimSequence && SelectedAnimSequence->GetDataModel())
                    {
                        MaxFrameSeconds = SelectedAnimSequence->GetPlayLength();
                        EndFrameSeconds = MaxFrameSeconds; // Set sequencer end to anim length

                        SelectedSkeletalMeshComponent->PlayAnimation(SelectedAnimSequence, false);
                        SelectedSkeletalMeshComponent->GetSingleNodeInstance()->GetCurrentSequence()->SetRateScale(0.0f);
                    }
                    else
                    {
                        MaxFrameSeconds = 0.0f;
                        EndFrameSeconds = 0.0f;
                    }
                }
            }
            if (is_selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

}

void AnimationSequenceViewer::PlayButton(bool* v) const
{
    // Not implement
}

void AnimationSequenceViewer::RepeatButton(bool* v) const
{
    ImVec4 ColorBg = *v ? ImVec4(0.0f, 0.3f, 0.0f, 1.0f) : ImVec4(0, 0, 0, 1.0f);
    
    ImGui::PushStyleColor(ImGuiCol_Button, ColorBg);
    if (ImGui::Button("\ue9d1", ImVec2(32, 32))) // Repeat
    {
        *v = !*v;
    }
    ImGui::PopStyleColor();
}

int AnimationSequenceViewer::FindAvailableTrackIndex(const TArray<FAnimNotifyEvent>& NotifyEvents) const
{
    TSet<int> UsedTrackIndex;

    for (const FAnimNotifyEvent& Notify : NotifyEvents)
    {
        UsedTrackIndex.Add(Notify.TrackIndex);
    }

    for (int i = 0;; ++i)
    {
        if (!UsedTrackIndex.Contains(i))
        {
            return i;
        }
    }
}
