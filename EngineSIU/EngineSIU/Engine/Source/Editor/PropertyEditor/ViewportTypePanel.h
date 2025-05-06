#pragma once
#include "IWindowToggleable.h"
#include "UnrealEd/EditorViewportClient.h"
class ViewportTypePanel : public IWindowToggleable
{
private:
    ViewportTypePanel() = default;

public:
    static ViewportTypePanel& GetInstance();

    void Draw(const std::shared_ptr<FEditorViewportClient>& ActiveViewport) const;
    void OnResize(HWND hWnd);

    virtual void Toggle() override { bWasOpen = !bWasOpen; }

private:
    bool bWasOpen = true;
    UINT width;
    UINT height;
};

