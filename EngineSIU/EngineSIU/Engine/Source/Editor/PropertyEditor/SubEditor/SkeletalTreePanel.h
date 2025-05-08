#pragma once
#include "Define.h"
#include "UnrealEd/EditorPanel.h"

class SkeletalTreePanel : public UEditorPanel
{
public:
    virtual void Render() override;
    virtual void OnResize(HWND hWnd) override;

private:
    void CreateSkeletalTreeNode();

private:
    float Width = 800, Height = 600;
};
