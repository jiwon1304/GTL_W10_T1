#pragma once

#pragma once
#include "IRenderPass.h"
#include "EngineBaseTypes.h"
#include "Container/Set.h"
#include "Define.h"
#include "Components/Light/PointLightComponent.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Engine/FbxObject.h"

class USkeletalMeshComponent;
struct FSkeletalMeshRenderData;
struct FSkeletalMaterial;
class FShadowManager;
class FDXDShaderManager;
class UWorld;
class UMaterial;
class FEditorViewportClient;

class FSkeletalMeshRenderPass : public virtual IRenderPass
{
public:
    FSkeletalMeshRenderPass();
    virtual ~FSkeletalMeshRenderPass();

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;

    virtual void PrepareRenderArr() override;
    void PrepareRenderState(std::shared_ptr<FEditorViewportClient> Viewport);
    void ChangeViewMode(EViewModeIndex ViewMode);
    void UpdateLitUnlitConstant(int32 isLit) const;
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;

    void RenderAllSkeletalMeshes(const std::shared_ptr<FEditorViewportClient>& Viewport);

    void UpdateObjectConstant(const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected, bool bCPUSkinning) const;
    void UpdateBoneMatrices(const TArray<FMatrix>& BoneMatrices) const;

    void SetCPUSkinning(bool bValue) { bCPUSkinning = bValue; }
private:
    void CreateShader();

    void UpdateVertexBuffer(FFbxMeshData& meshData, const TArray<FMatrix>& BoneMatrices);
    void GetSkinnedVertices(USkeletalMesh* SkeletalMesh, uint32 Section, const TArray<FMatrix>& BoneMatrices, TArray<FSkeletalVertex>& OutVertices) const;
    bool bCPUSkinning = false;

protected:
    TArray<USkeletalMeshComponent*> SkeletalMeshComponents;

    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
    FDXDShaderManager* ShaderManager;

};
