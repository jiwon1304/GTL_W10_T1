#include "SkinnedMeshRenderPass.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"


FSkinnedMeshRenderPass::FSkinnedMeshRenderPass()
    : BufferManager(nullptr)
    , Graphics(nullptr)
    , ShaderManager(nullptr)
{
}

void FSkinnedMeshRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManage;
}

void FSkinnedMeshRenderPass::PrepareRenderArr()
{
    for (USkinnedMeshComponent* Component : TObjectRange<USkinnedMeshComponent>())
    {
        if (Component->GetWorld() == GEngine->ActiveWorld)
        {
            if (Component->GetOwner() && !Component->GetOwner()->IsHidden())
            {
                SkinnedMeshComponents.Add(Component);
            }
        }
    }
}

void FSkinnedMeshRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    for (USkinnedMeshComponent* Component : SkinnedMeshComponents)
    {
        if (!Component)
        {
            continue;
        }

        // TODO: Implement rendering logic for each skinned mesh component.
    }
}

void FSkinnedMeshRenderPass::ClearRenderArr()
{
    SkinnedMeshComponents.Empty();
}
