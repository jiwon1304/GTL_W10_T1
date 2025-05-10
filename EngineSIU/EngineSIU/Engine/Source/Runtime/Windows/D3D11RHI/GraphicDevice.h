#pragma once
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "dxgi.lib")


#define _TCHAR_DEFINED
#include <d3d11.h>
#include <dxgi.h>

#include "EngineBaseTypes.h"

#include "Core/HAL/PlatformType.h"
#include "Core/Math/Vector4.h"
#include "Container/Map.h"

class FEditorViewportClient;

class FGraphicsDevice
{
public:
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;

    // 멀티 윈도우 지원을 위한 맵
    TMap<HWND, IDXGISwapChain*> SwapChains;
    
    TMap<HWND, ID3D11Texture2D*> BackBufferTextures;
    TMap<HWND, ID3D11RenderTargetView*> BackBufferRTVs;
    
    ID3D11RasterizerState* RasterizerSolidBack = nullptr;
    ID3D11RasterizerState* RasterizerSolidFront = nullptr;
    ID3D11RasterizerState* RasterizerWireframeBack = nullptr;
    ID3D11RasterizerState* RasterizerShadow = nullptr;

    ID3D11DepthStencilState* DepthStencilStateTestLess = nullptr;
    ID3D11DepthStencilState* DepthStencilStateTestAlways = nullptr;
    
    ID3D11BlendState* AlphaBlendState = nullptr;
    
    DXGI_SWAP_CHAIN_DESC SwapchainDesc;
    
    TMap<HWND, UINT> ScreenWidths;
    TMap<HWND, UINT> ScreenHeights;

    TMap<HWND, D3D11_VIEWPORT> Viewports;
    
    FLOAT ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // 화면을 초기화(clear) 할 때 사용할 색상(RGBA)

    void Initialize(HWND hWnd);
    void CreateAdditionalSwapChain(HWND hWnd);
    
    void ChangeRasterizer(EViewModeIndex ViewModeIndex);
    //void CreateRTV(ID3D11Texture2D*& OutTexture, ID3D11RenderTargetView*& OutRTV);
    ID3D11Texture2D* CreateTexture2D(const D3D11_TEXTURE2D_DESC& Description, const void* InitialData);
    
    void Release();
    
    void Prepare(HWND hWnd);

    void SwapBuffer(HWND hWnd) const;
    
    void Resize(HWND hWnd);
    
    ID3D11RasterizerState* GetCurrentRasterizer() const { return CurrentRasterizer; }

    /*
    uint32 GetPixelUUID(POINT pt) const;
    uint32 DecodeUUIDColor(FVector4 UUIDColor) const;
    */
    
private:
    void CreateDeviceAndSwapChain(HWND hWnd, bool bIsInitialDevice);
    void CreateBackBuffer(HWND hWnd);
    void CreateDepthStencilState();
    void CreateRasterizerState();
    void CreateAlphaBlendState();
    
    void ReleaseDeviceAndSwapChain();
    void ReleaseSwapChain(HWND hWnd);
    void ReleaseAllSwapChains();
    void ReleaseFrameBuffer(HWND hWnd);
    void ReleaseAllFrameBuffers();
    void ReleaseRasterizerState();
    void ReleaseDepthStencilResources();
    
    ID3D11RasterizerState* CurrentRasterizer = nullptr;

    const DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    const DXGI_FORMAT BackBufferRTVFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
};

