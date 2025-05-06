#include "AssetViewerWindow.h"
#include "Widgets/SWindow.h" // Assuming SlateCore module
#include "Launch/EngineLoop.h" // Assuming Launch module, adjust if needed

// Helper to get HINSTANCE if not directly available
// In a full UE project, FPlatformApplicationMisc::GetApplicationInstance() would be used.
// For this specific project structure, GEngineLoop might be an option if accessible and appropriate.
// Assuming FPlatformApplicationMisc is the more standard way.
extern FEngineLoop GEngineLoop; // If we need to access GEngineLoop.AppWnd for parent or hInstance

FAssetViewerWindow::FAssetViewerWindow()
    : OSWindowHandle(nullptr)
    , bIsInitialized(false)
    , bIsVisible(false)
{
}

FAssetViewerWindow::~FAssetViewerWindow()
{
    if (OSWindowHandle)
    {
        DestroyWindow(OSWindowHandle);
        OSWindowHandle = nullptr;
    }
}

void FAssetViewerWindow::Initialize(HINSTANCE hInstance)
{
    if (!bIsInitialized)
    {
        if (CreateOSWindow(hInstance))
        {
            bIsInitialized = true;
        }
    }
}

void FAssetViewerWindow::ToggleWindow(HINSTANCE hInstance)
{
    if (!bIsInitialized)
    {
        Initialize(hInstance);
    }

    if (bIsVisible)
    {
        Hide();
    }
    else
    {
        Show();
    }
}

void FAssetViewerWindow::Show()
{
    if (OSWindowHandle && !bIsVisible)
    {
        ShowWindow(OSWindowHandle, SW_SHOWDEFAULT);
        UpdateWindow(OSWindowHandle);
        bIsVisible = true;
    }
}

void FAssetViewerWindow::Hide()
{
    if (OSWindowHandle && bIsVisible)
    {
        ShowWindow(OSWindowHandle, SW_HIDE);
        bIsVisible = false;
    }
}

bool FAssetViewerWindow::CreateOSWindow(HINSTANCE hInstance)
{
    if (!hInstance)
    {
        // Fallback or error if HINSTANCE cannot be obtained
        // For this project, GEngineLoop.AppWnd might be used to get HINSTANCE via GetWindowLongPtr(GWLP_HINSTANCE)
        // but FPlatformApplicationMisc is preferred if available and correctly set up.
        // If GEngineLoop is the only way:
        // HInstance = (HINSTANCE)GetWindowLongPtr(GEngineLoop.AppWnd, GWLP_HINSTANCE);
        // This is a simplification; a real engine would have a robust way to get this.
        UE_LOG(ELogLevel::Error, TEXT("Failed to get application instance for AssetViewerWindow."));
        return false;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = FAssetViewerWindow::AssetViewerWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WindowClassName;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    // Optional: Add an icon
    // wc.hIcon = LoadIcon(HInstance, MAKEINTRESOURCE(YourIconID)); 
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc))
    {
        // Check if already registered
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            UE_LOG(ELogLevel::Error, TEXT("Failed to register window class for AssetViewerWindow. Error: %d"), GetLastError());
            return false;
        }
    }

    // Create the window
    OSWindowHandle = CreateWindowExW(
        0,                              // Optional window styles.
        WindowClassName,                // Window class
        L"Asset Viewer",                // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,

        nullptr,                        // Parent window (nullptr for top-level)
        nullptr,                        // Menu
        hInstance,                      // Instance handle
        this                            // Additional application data (pass 'this' to retrieve in WM_CREATE)
    );

    if (!OSWindowHandle)
    {
        UE_LOG(ELogLevel::Error, TEXT("Failed to create OS window for AssetViewerWindow. Error: %d"), GetLastError());
        return false;
    }

    // Do not show window immediately, Show() will handle this.
    // bIsVisible = false; // Already initialized to false

    return true;
}

LRESULT CALLBACK FAssetViewerWindow::AssetViewerWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    FAssetViewerWindow* ThisWindow = nullptr;

    if (message == WM_NCCREATE)
    {
        CREATESTRUCT* CreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        ThisWindow = static_cast<FAssetViewerWindow*>(CreateStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ThisWindow));
    }
    else
    {
        ThisWindow = reinterpret_cast<FAssetViewerWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (ThisWindow)
    {
        switch (message)
        {
        case WM_CLOSE:
            if (ThisWindow)
            {
                ThisWindow->Hide();
            }
            return 0; // Indicate we've handled the message

        case WM_DESTROY:
            //SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); // Clear user data
            // For a secondary window, we don't call PostQuitMessage(0) as that exits the app.
            return 0;

        case WM_SIZE:
            break;
        
        // Add other message handlers as needed (e.g., WM_ACTIVATE, WM_PAINT for custom non-Slate painting)
        }
    }

    // Pass unhandled messages to DefWindowProc
    return DefWindowProc(hwnd, message, wParam, lParam);
}

bool FAssetViewerWindow::IsVisible() const
{
    return bIsVisible && OSWindowHandle && ::IsWindowVisible(OSWindowHandle);
}
