#pragma once
#include "GameFramework/Actor.h"
#include "ImGui/imgui_neo_sequencer.h"
#include "UnrealEd/EditorPanel.h"

struct FAnimNotifyEvent;
class USkeletalMeshComponent;
class UAnimSequence;

class AnimationSequenceViewer : public UEditorPanel
{
public:
    virtual void Render() override;
    virtual void OnResize(HWND hWnd) override;

private:
    void RenderAnimationSequence(float InWidth, float InHeight);
    void RenderPlayController(float InWidth, float InHeight);
    void RenderAssetDetails();
    void RenderAssetBrowser();

    void PlayButton(bool* v) const;
    void RepeatButton(bool* v) const;

private:
    int FindAvailableTrackIndex(const TArray<FAnimNotifyEvent>& Notifies);
    
private:
    float Width = 800, Height = 600;

    float CurrentFrameSeconds = 0.0f;
    float StartFrameSeconds = 0.0f;
    float EndFrameSeconds = 1.0f;
    float MaxFrameSeconds = 1.0f;

    int CurrentFrame = 0;
    int StartFrame = 0;
    int EndFrame = 0;
    
    bool bIsPlaying = false;
    bool bIsRepeating = false;

    /* Animation Property for Debug */
    int SelectedAnimIndex = -1;
    FString SelectedAnimName;
    
    UAnimSequence* SelectedAnimSequence = nullptr;
    USkeletalMeshComponent* SelectedSkeletalMeshComponent = nullptr;
    

private:
    int SelectedTrackIndex = -1;
    int SelectedNotifyIndex = -1;
    bool bNeedsNotifyUpdate = false;
    
    
    TMap<int32, TArray<ImGui::FrameIndexType>> TrackAndKeyMap;
    TSet<int32> TrackList;
};
