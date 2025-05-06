#pragma once
#include "IRenderPass.h"
#include "Container/Array.h"

class USkinnedMeshComponent;


class FSkinnedMeshRenderPass : public IRenderPass
{
public:
    FSkinnedMeshRenderPass();
    virtual ~FSkinnedMeshRenderPass() override = default;

    // 이동 & 복사 생성자 제거
    FSkinnedMeshRenderPass(const FSkinnedMeshRenderPass&) = delete;
    FSkinnedMeshRenderPass& operator=(const FSkinnedMeshRenderPass&) = delete;
    FSkinnedMeshRenderPass(FSkinnedMeshRenderPass&&) = delete;
    FSkinnedMeshRenderPass& operator=(FSkinnedMeshRenderPass&&) = delete;

public:
    // IRenderPass
    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) override;
    virtual void PrepareRenderArr() override;
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;
    // ~IRenderPass

protected:
    TArray<USkinnedMeshComponent*> SkinnedMeshComponents;

    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
    FDXDShaderManager* ShaderManager;
};
