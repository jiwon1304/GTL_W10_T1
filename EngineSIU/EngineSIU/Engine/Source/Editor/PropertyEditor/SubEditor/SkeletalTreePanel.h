#pragma once
#include "Define.h"
#include "UnrealEd/EditorPanel.h"

class USkeletalMeshComponent;

class SkeletalTreePanel : public UEditorPanel
{
public:
    virtual void Render() override;
    virtual void OnResize(HWND hWnd) override;

private:
    void RenderSkeletalTree(int InParentIndex, USkeletalMeshComponent* SkeletalMeshComponent, const TMap<int, FString>& BoneIndexToName);
    void CreateSkeletalTreeNode();
    void CreateSkeletalDetail();
    void DrawAnimationControls();

private:
    float Width = 800, Height = 600;

    USkeletalMeshComponent* SelectedSkeleton = nullptr;
    int SelectedBoneIndex = -1;
    
};
