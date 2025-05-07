
#pragma once

#define _TCHAR_DEFINED
#include <d3d11.h>
#include <memory>

#include "IRenderPass.h"
#include "Math/Color.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Engine/Classes/Components/HeightFogComponent.h"

class FGraphicsDevice;
class FDXDShaderManager;
class FDXDBufferManager;
class FEditorViewportClient;

class FLightHeatMapRenderPass : public IRenderPass
{
public:
    FLightHeatMapRenderPass();
    virtual ~FLightHeatMapRenderPass() override;
    
    // 초기화: 그래픽 디바이스와 셰이더 매니저를 등록
    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;

    // Fog 렌더링용 셰이더 생성 및 입력 레이아웃 설정
    void CreateShader();

    virtual void PrepareRenderArr() override;

    virtual void ClearRenderArr() override;

    void PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport);
    // Fog를 화면에 렌더링
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    void CreateBlendState();

    void FinalRender();

    void SetDebugHeatmapSRV(ID3D11ShaderResourceView* InDebugHeatmapSRV);
    

private:
    ID3D11SamplerState* Sampler = nullptr;

    FGraphicsDevice* Graphics;
    FDXDBufferManager* BufferManager;
    FDXDShaderManager* ShaderManager;

    // Fog 렌더링용 셰이더 및 입력 레이아웃
    ID3D11VertexShader* FogVertexShader;
    ID3D11PixelShader* FogPixelShader;
    ID3D11PixelShader* FogQuadPixelShader;
    ID3D11InputLayout* InputLayout;

    // Scene SRV (외부에서 등록)
    ID3D11ShaderResourceView* DebugHeatmapSRV; // 외부에서 등록

    ID3D11Texture2D* FogBuffer = nullptr;
    ID3D11RenderTargetView* FogRTV = nullptr;

    ID3D11BlendState* FogBlendState = nullptr;

    TArray<UHeightFogComponent*> FogComponents;

    float screenWidth = 0;
    float screenHeight = 0;
};
