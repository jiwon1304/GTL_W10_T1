#pragma once

#pragma once
#include "IRenderPass.h"
#include "EngineBaseTypes.h"
#include "Container/Set.h"
#include "Define.h"
#include "Components/Light/PointLightComponent.h"

struct FSkeletalMeshRenderData;
class FShadowManager;
class FDXDShaderManager;
class UWorld;
class UMaterial;
class FEditorViewportClient;
class USkeletalMeshComponent;
struct FSkeletalMaterial;

class FSkeletalMeshRenderPass : public IRenderPass
{
public:
    FSkeletalMeshRenderPass();
    virtual ~FSkeletalMeshRenderPass();

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;

    virtual void PrepareRenderArr() override;
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;

    void RenderAllSkeletalMeshes(const std::shared_ptr<FEditorViewportClient>& Viewport);

    void UpdateObjectConstant(const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected) const;
    void UpdateBoneMatrices(const TArray<FMatrix>& BoneMatrices) const;

private:
    void CreateShader();
protected:
    TArray<USkeletalMeshComponent*> SkeletalMeshComponents;

    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
    FDXDShaderManager* ShaderManager;

};
