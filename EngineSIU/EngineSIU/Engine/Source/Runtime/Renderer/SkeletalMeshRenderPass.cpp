#include "SkeletalMeshRenderPass.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"


FSkeletalMeshRenderPass::FSkeletalMeshRenderPass()
    : BufferManager(nullptr)
    , Graphics(nullptr)
    , ShaderManager(nullptr)
{
}

void FSkeletalMeshRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManage;
}

void FSkeletalMeshRenderPass::PrepareRenderArr()
{
    for (USkeletalMeshComponent* Component : TObjectRange<USkeletalMeshComponent>())
    {
        if (Component->GetWorld() == GEngine->ActiveWorld)
        {
            if (Component->GetOwner() && !Component->GetOwner()->IsHidden())
            {
                SkeletalMeshComponents.Add(Component);
            }
        }
    }
}

void FSkeletalMeshRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    for (USkeletalMeshComponent* Component : SkeletalMeshComponents)
    {
        if (!Component)
        {
            continue;
        }
    }
}

void FSkeletalMeshRenderPass::ClearRenderArr()
{
    SkeletalMeshComponents.Empty();
}
