#pragma once
#include "GameFramework/Actor.h"
#include "UnrealEd/EditorPanel.h"

class AnimationSequenceViewer : public UEditorPanel
{
public:
    virtual void Render() override;
    virtual void OnResize(HWND hWnd) override;

private:
    void RenderAnimationSequence(float Width, float Height) const;
    void RenderPlayController(float Width, float Height) const;
    void RenderAssetDetails() const;
    void RenderAssetBrowser() const;

    void PlayButton(bool* v) const;
    void RepeatButton(bool* v) const;
private:
    float Width = 800, Height = 600;
};
