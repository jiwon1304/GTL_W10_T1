#pragma once
#include "IRenderPass.h"
#include "D3D11RHI/GraphicDevice.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "D3D11RHI/DXDBufferManager.h"

class FCameraEffectRenderPass : public IRenderPass
{
public:
    FCameraEffectRenderPass();
    virtual ~FCameraEffectRenderPass() override;

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;
    void ReleaseShader();
    void CreateShader();
    void UpdateShader();
    void CreateBlendState();
    virtual void PrepareRenderArr() override {}
    virtual void ClearRenderArr() override {}
    void CreateSampler();
    void PrepareRenderState();

    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

private:
    FGraphicsDevice* Graphics = nullptr;
    FDXDBufferManager* BufferManager = nullptr;
    FDXDShaderManager* ShaderManager = nullptr;

    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;
    ID3D11BlendState* BlendState = nullptr;
    ID3D11SamplerState* Sampler = nullptr;
};
