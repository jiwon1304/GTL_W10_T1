#include "AnimationSequenceViewer.h"

#include "Components/Mesh/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimNotifies/AnimNotifyState_SlowMotion.h"
#include "Contents/Actors/ItemActor.h"
#include "Engine/EditorEngine.h"
#include "Engine/FbxManager.h"

//#define TEST_NOTIFY_STATE

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

    // Update CurrentFrameSeconds if Playing
    if (bIsPlaying && SelectedAnimSequence && SelectedAnimSequence->GetDataModel())
    {
        float DeltaTime = ImGui::GetIO().DeltaTime;
        float RateScale = SelectedAnimSequence->GetRateScale();
        CurrentFrameSeconds += DeltaTime * RateScale;

        if (RateScale >= 0.0f)
        {
            // Forward playback
            if (CurrentFrameSeconds >= MaxFrameSeconds)
            {
                if (bIsRepeating)
                {
                    CurrentFrameSeconds = fmodf(CurrentFrameSeconds, MaxFrameSeconds);
                }
                else
                {
                    CurrentFrameSeconds = MaxFrameSeconds;
                    bIsPlaying = false;
                }
            }
        }
        else
        {
            // Reverse playback
            if (CurrentFrameSeconds < 0.0f)
            {
                if (bIsRepeating)
                {
                    float Wrapped = fmodf(-CurrentFrameSeconds, MaxFrameSeconds);
                    CurrentFrameSeconds = MaxFrameSeconds - Wrapped;
                    if (CurrentFrameSeconds >= MaxFrameSeconds)
                    {
                        CurrentFrameSeconds -= MaxFrameSeconds;
                    }
                }
                else
                {
                    CurrentFrameSeconds = EndFrameSeconds;
                    bIsPlaying = false;
                }
            }
        }

        if (SelectedSkeletalMeshComponent)
        {
            auto* NodeInstance = SelectedSkeletalMeshComponent->GetSingleNodeInstance();
            NodeInstance->SetPlaying(bIsPlaying);
            NodeInstance->SetCurrentTime(CurrentFrameSeconds);
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
    TArray<FAnimNotifyEvent>& Notifies = SelectedAnimSequence->Notifies;

    // TrackList는 사용자가 추가/삭제한 트랙을 직접 관리한다.
    // Notifies를 기반으로는 TrackAndKeyMap만 동기화한다.
    if (SelectedAnimSequence && SelectedAnimSequence->GetDataModel() && bNeedsNotifyUpdate)
    {
        TrackAndKeyMap.Empty();

        // 1. Notifies에 있는 모든 TrackIndex를 TrackList에 포함시킴
        for (const auto& Notify : Notifies)
        {
            if (!TrackList.Contains(Notify.TrackIndex))
            {
                TrackList.Add(Notify.TrackIndex);
            }
            TrackAndKeyMap.FindOrAdd(Notify.TrackIndex).Add(Notify.TriggerTime);
        }

        bNeedsNotifyUpdate = false;
    }

    if (ImGui::BeginNeoSequencer("Sequencer", &CurrentFrameSeconds, &StartFrameSeconds, &EndFrameSeconds, { InWidth, InHeight },
        ImGuiNeoSequencerFlags_EnableSelection |
        ImGuiNeoSequencerFlags_Selection_EnableDragging |
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
#ifdef TEST_NOTIFY_STATE
                        NewNotify.Duration = 2.0f;
                        NewNotify.NotifyName = "SLOW_MOTION";
                        UAnimNotifyState_SlowMotion* NewNotifyState = FObjectFactory::ConstructObject<UAnimNotifyState_SlowMotion>(nullptr);
                        NewNotify.NotifyStateClass = Cast<UAnimNotifyState>(NewNotifyState);
#endif
                        Notifies.Add(NewNotify);
                        bNeedsNotifyUpdate = true;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }

            ImGui::EndNeoGroup();
        }

        if (SelectedSkeletalMeshComponent && SelectedSkeletalMeshComponent->GetSingleNodeInstance() != nullptr)
        {
            SelectedSkeletalMeshComponent->GetSingleNodeInstance()->SetCurrentTime(CurrentFrameSeconds);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete))
        {
            UE_LOG(ELogLevel::Display, "Delete Key Clicked");
            if (SelectedNotifyIndex >= 0 && SelectedNotifyIndex < Notifies.Num())
            {
                Notifies.RemoveAt(SelectedNotifyIndex);
                SelectedNotifyIndex = -1;
                bNeedsNotifyUpdate = true;
            }
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
        SingleNode->SetCurrentTime(0.0f);
        SingleNode->SetPlaying(false);
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
                SingleNode->SetPlaying(true);
            }
            else 
            {
                // Play
                if (CurrentFrameSeconds >= MaxFrameSeconds && MaxFrameSeconds > 0.0f)
                {
                    CurrentFrameSeconds = 0.0f;
                    SingleNode->SetCurrentTime(0.0f);
                }

                // SelectedSkeletalMeshComponent->PlayAnimation(SelectedAnimSequence, bIsRepeating);
            }
        }
        else
        {
            // Pause
            if (SingleNode != nullptr)
            {
                SingleNode->SetPlaying(false);
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("\ue96a", IconSize)) // Fast-forward
    {
        if (SelectedAnimSequence && SelectedAnimSequence->GetDataModel())
        {
            CurrentFrameSeconds = MaxFrameSeconds;
            SingleNode->SetCurrentTime(CurrentFrameSeconds);
        }
        
        if (!bIsRepeating)
        {
            bIsPlaying = false; // Stop Play
            SingleNode->SetPlaying(false);
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
    if (SelectedAnimSequence)
    {
        float RateScale = SelectedAnimSequence->GetRateScale();
        ImGui::Text("Rate Scale ");
        ImGui::SameLine();
        if (ImGui::DragFloat("##RateScale", &RateScale, 0.01f, -100.0f, 100.0f, "%.2f"))
        {
            SelectedAnimSequence->SetRateScale(RateScale);
        }

        ImGui::Text("Loop ");
        ImGui::SameLine();
        bool* bLoop = &SelectedAnimSequence->bLoop;
        if (ImGui::Checkbox("##Loop", bLoop))
        {
            
        }

        ImGui::Separator();
    }
    
    if (SelectedNotifyIndex == -1)
    {
        return;
    }

    FAnimNotifyEvent& Notify = SelectedAnimSequence->Notifies[SelectedNotifyIndex];
    
    static char NotifyBuffer[128] = { 0 };
    
    std::string NotifyNameStr = GetData(*Notify.NotifyName.ToString());
    strncpy_s(NotifyBuffer, NotifyNameStr.c_str(), sizeof(NotifyBuffer));
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
        FSpinLockGuard Lock(FFbxManager::AnimMapLock);
        for (auto const& [name, entry] : FFbxManager::GetAnimSequences())
        {
            if (entry.State == FFbxManager::LoadState::Completed && entry.Sequence != nullptr)
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

                UAnimSequence* newSequence = FFbxManager::GetAnimSequenceByName(SelectedAnimName);
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

                        SelectedSkeletalMeshComponent->SetAnimation(SelectedAnimSequence);
                        SelectedSkeletalMeshComponent->GetSingleNodeInstance()->SetCurrentTime(0.0f);
                        SelectedSkeletalMeshComponent->GetSingleNodeInstance()->SetPlaying(false);
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

int AnimationSequenceViewer::FindAvailableTrackIndex(const TArray<FAnimNotifyEvent>& Notifies)
{
    TSet<int32> UsedIndices;
    for (const FAnimNotifyEvent& Notify : Notifies)
    {
        UsedIndices.Add(Notify.TrackIndex);
    }
    for (int32 TrackIdx : TrackList)
    {
        UsedIndices.Add(TrackIdx);
    }

    int32 NewIndex = 0;
    while (UsedIndices.Contains(NewIndex))
    {
        ++NewIndex;
    }
    return NewIndex;
}
