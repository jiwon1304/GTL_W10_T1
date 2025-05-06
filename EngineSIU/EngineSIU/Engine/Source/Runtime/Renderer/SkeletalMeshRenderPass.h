#pragma once
#include "IRenderPass.h"
#include "Container/Array.h"

class USkeletalMeshComponent;


class FSkeletalMeshRenderPass : public IRenderPass
{
public:
    FSkeletalMeshRenderPass();
    virtual ~FSkeletalMeshRenderPass() override = default;

    // 이동 & 복사 생성자 제거
    FSkeletalMeshRenderPass(const FSkeletalMeshRenderPass&) = delete;
    FSkeletalMeshRenderPass& operator=(const FSkeletalMeshRenderPass&) = delete;
    FSkeletalMeshRenderPass(FSkeletalMeshRenderPass&&) = delete;
    FSkeletalMeshRenderPass& operator=(FSkeletalMeshRenderPass&&) = delete;

public:
    // IRenderPass
    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) override;
    virtual void PrepareRenderArr() override;
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;
    // ~IRenderPass

protected:
    TArray<USkeletalMeshComponent*> StaticMeshComponents;

    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
};
