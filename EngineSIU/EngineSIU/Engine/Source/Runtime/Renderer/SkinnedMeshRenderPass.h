#pragma once
#include "EngineBaseTypes.h"
#include "IRenderPass.h"
#include "Container/Array.h"
#include "Math/Matrix.h"

class USkeletalMesh;
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

    void CreateShader() const;
    void PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport) const;
    void ChangeViewMode(EViewModeIndex ViewMode) const;
    void UpdateLitUnlitConstant(bool bIsLit) const;
    void RenderAllSkinnedMeshes();
    void UpdateObjectConstant(
        const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected, bool bCPUSkinning
    ) const;
    void UpdateBoneMatrices(const TArray<FMatrix>& BoneMatrices) const;
    void UpdateVertexBuffer(const USkeletalMesh& MeshData, const TArray<FMatrix>& BoneMatrices) const;

protected:
    TArray<USkinnedMeshComponent*> SkinnedMeshComponents;

    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
    FDXDShaderManager* ShaderManager;

private:
    bool bIsCPUSkinning = true;
};
