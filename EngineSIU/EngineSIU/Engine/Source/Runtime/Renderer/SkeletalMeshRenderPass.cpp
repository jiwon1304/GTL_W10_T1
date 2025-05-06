#include "SkeletalMeshRenderPass.h"

FSkeletalMeshRenderPass::FSkeletalMeshRenderPass()
    : BufferManager(nullptr)
    , Graphics(nullptr)
{
}

void FSkeletalMeshRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage)
{
}

void FSkeletalMeshRenderPass::PrepareRenderArr()
{
}

void FSkeletalMeshRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
}

void FSkeletalMeshRenderPass::ClearRenderArr()
{
}
