#pragma once
#include "GameFramework/Actor.h"

class FAssetViewerWindow
{
public:
    FAssetViewerWindow();
    ~FAssetViewerWindow();

    void Initialize(HINSTANCE hInstance);
    void ToggleWindow(HINSTANCE hInstance);
    void Show();
    void Hide();

    bool IsVisible() const;

private:
    bool CreateOSWindow(HINSTANCE hInstance);

    static LRESULT CALLBACK AssetViewerWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HWND OSWindowHandle;
    bool bIsInitialized;
    bool bIsVisible;
    const wchar_t* WindowClassName = L"AssetViewerWindowClass";
};
