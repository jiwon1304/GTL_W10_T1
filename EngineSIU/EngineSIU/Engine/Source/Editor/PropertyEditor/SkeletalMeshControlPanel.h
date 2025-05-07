#pragma once

#include "Components/ActorComponent.h"
#include "UnrealEd/EditorPanel.h"
#include "ControlEditorPanel.h"

// SkeletalMeshViewer에서 사용할 컨트롤 패널 클래스
// 기존 ControlEditorPanel을 상속받아 스켈레탈 메시 특화 기능을 추가함
class SkeletalMeshControlPanel : public ControlEditorPanel
{
public:
    SkeletalMeshControlPanel() : Width(300), Height(100) {}
    
    // UI 렌더링
    virtual void Render() override;
    
    // 윈도우 리사이징 처리
    virtual void OnResize(HWND hWnd) override;

private:
    float Width;
    float Height;
    
    // 메뉴 버튼 생성 (스켈레탈 메시 특화 메뉴)
    void CreateSkeletalMenuButton(const ImVec2 ButtonSize, ImFont* IconFont);
    
    // 스켈레탈 메시 관련 컨트롤 생성
    void CreateSkeletalMeshControls(const ImVec2 ButtonSize);
    
    // 애니메이션 컨트롤 생성 (재생/정지/스크러빙 등)
    void CreateAnimationControls(const ImVec2 ButtonSize, ImFont* IconFont);
    
    // 본 표시 설정
    void CreateBoneDisplayOptions();

    // 모달 창 상태
    bool bShowBoneOptions = false;
    bool bShowAnimationControls = false;
};
