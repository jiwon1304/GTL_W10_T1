#include "SkeletalMeshRenderPass.h"
#include "EngineLoop.h"
#include "World/World.h"
#include "RendererHelpers.h"
#include "ShadowManager.h"
#include "UnrealClient.h"
#include "Math/JungleMath.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Casts.h"
#include "D3D11RHI/DXDBufferManager.h"
#include "D3D11RHI/GraphicDevice.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "Engine/EditorEngine.h"
#include "PropertyEditor/ShowFlags.h"
#include "UnrealEd/EditorViewportClient.h"

FSkeletalMeshRenderPass::FSkeletalMeshRenderPass()
    : BufferManager(nullptr), Graphics(nullptr), ShaderManager(nullptr)
{
}

FSkeletalMeshRenderPass::~FSkeletalMeshRenderPass()
{
}

void FSkeletalMeshRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManager;
    CreateShader();
}

void FSkeletalMeshRenderPass::PrepareRenderArr()
{
    for (const auto& Component : TObjectRange<USkeletalMeshComponent>())
    {
        SkeletalMeshComponents.Add(Component);
    }
}

void FSkeletalMeshRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
     ShaderManager->SetVertexShaderAndInputLayout(L"SkeletalMeshVertexShader", Graphics->DeviceContext);
     BufferManager->BindConstantBuffer("FBoneMatrices", 11, EShaderStage::Vertex);

    //for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
    //{
    //    if (!SkeletalMeshComponent || !SkeletalMeshComponent->GetSkeletalMesh())
    //    {
    //        continue;
    //    }

    //    FSkeletalMeshRenderData* RenderData = SkeletalMeshComponent->GetSkeletalMesh()->GetRenderData();
    //    if (!RenderData)
    //    {
    //        continue;
    //    }

    //    // Calculate bone matrices on the CPU
    //    TArray<FMatrix> BoneMatrices;
    //    SkeletalMeshComponent->CalculateBoneMatrices(BoneMatrices);

    //    // Update constant buffers
    //    UpdateObjectConstant(SkeletalMeshComponent->GetWorldMatrix(), SkeletalMeshComponent->EncodeUUID() / 255.0f, false);
    //    UpdateBoneMatrices(BoneMatrices);

    //    // Render the skeletal mesh
    //}
    RenderAllSkeletalMeshes(Viewport);

    //ClearRenderArr();
}

void FSkeletalMeshRenderPass::RenderAllSkeletalMeshes(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
    {
        if (!SkeletalMeshComponent) continue;
        if (!SkeletalMeshComponent->GetSkeletalMesh()) continue;

        FSkeletalMeshRenderData* RenderData = SkeletalMeshComponent->GetSkeletalMesh()->GetRenderData();
        if (!RenderData) continue;

        // Bone Matrix는 CPU에서 처리
        TArray<FMatrix> BoneMatrices;
        SkeletalMeshComponent->CalculateBoneMatrices(BoneMatrices);

        // Update constant buffers
        UpdateObjectConstant(
            SkeletalMeshComponent->GetWorldMatrix(), 
            SkeletalMeshComponent->EncodeUUID() / 255.0f,
            SkeletalMeshComponent->IsActive(),
            RenderData->bCPUSkinning
        );

        TArray<FSkeletalMaterial*> Materials = SkeletalMeshComponent->GetSkeletalMesh()->GetMaterials();
        TArray<UMaterial*> OverrideMaterials = SkeletalMeshComponent->GetOverrideMaterials();

        FVertexInfo VertexInfo;
        if (RenderData->bCPUSkinning)
        {
            BufferManager->CreateDynamicVertexBuffer(RenderData->ObjectName, RenderData->Vertices, VertexInfo);
            UpdateVertexBuffer(RenderData, BoneMatrices);
        }
        else
        {
            BufferManager->CreateVertexBuffer(RenderData->ObjectName, RenderData->Vertices, VertexInfo);
            UpdateBoneMatrices(BoneMatrices);
        }

        Graphics->DeviceContext->IASetVertexBuffers(0, 1, &VertexInfo.VertexBuffer, &VertexInfo.Stride, &VertexInfo.Offset);

        FIndexInfo IndexInfo;
        BufferManager->CreateIndexBuffer(RenderData->ObjectName, RenderData->Indices, IndexInfo);
        if (IndexInfo.IndexBuffer)
        {
            Graphics->DeviceContext->IASetIndexBuffer(IndexInfo.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        }

        
        // 이제 기본 subset이 있음
        //Graphics->DeviceContext->DrawIndexed(IndexInfo.NumIndices, 0, 0);

        for (int SubMeshIndex = 0; SubMeshIndex < RenderData->MaterialSubsets.Num(); SubMeshIndex++)
        {
            uint32 MaterialIndex = RenderData->MaterialSubsets[SubMeshIndex].MaterialIndex;

            if (MaterialIndex < OverrideMaterials.Num() && OverrideMaterials[MaterialIndex] != nullptr)
            {
                MaterialUtils::UpdateMaterial(BufferManager, Graphics, OverrideMaterials[MaterialIndex]->GetMaterialInfo());
            }
            else
            {
                MaterialUtils::UpdateMaterial(BufferManager, Graphics, Materials[MaterialIndex]->Material->GetMaterialInfo());
            }

            uint32 StartIndex = RenderData->MaterialSubsets[SubMeshIndex].IndexStart;
            uint32 IndexCount = RenderData->MaterialSubsets[SubMeshIndex].IndexCount;
            Graphics->DeviceContext->DrawIndexed(IndexCount, StartIndex, 0);
        }
    }
}


void FSkeletalMeshRenderPass::UpdateObjectConstant(const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected, bool bCPUSkinning) const
{
    FObjectConstantBuffer ObjectData = {};
    ObjectData.WorldMatrix = WorldMatrix;
    ObjectData.InverseTransposedWorld = FMatrix::Transpose(FMatrix::Inverse(WorldMatrix));
    ObjectData.UUIDColor = UUIDColor;
    ObjectData.bIsSelected = bIsSelected;
    ObjectData.bCPUSkinning = bCPUSkinning? 1 : 0;

    BufferManager->UpdateConstantBuffer(TEXT("FObjectConstantBuffer"), ObjectData);
}

void FSkeletalMeshRenderPass::UpdateBoneMatrices(const TArray<FMatrix>& BoneMatrices) const
{
    BufferManager->UpdateConstantBuffer(TEXT("FBoneMatrices"), BoneMatrices);
}

void FSkeletalMeshRenderPass::CreateShader()
{
    HRESULT hr = ShaderManager->AddVertexShaderAndInputLayout(L"SkeletalMeshVertexShader", L"Shaders/SkeletalMeshVertexShader.hlsl", "mainVS", FSkeletalMeshRenderData::LayoutDesc, ARRAYSIZE(FSkeletalMeshRenderData::LayoutDesc));
    if (FAILED(hr))
    {
        return;
    }
}

void FSkeletalMeshRenderPass::UpdateVertexBuffer(FSkeletalMeshRenderData* RenderData, const TArray<FMatrix>& BoneMatrices)
{
    TArray<FSkeletalMeshVertex> NewVertices;
    NewVertices.Reserve(RenderData->Vertices.Num());

    for (const FSkeletalMeshVertex& Vertex : RenderData->Vertices)
    {
        FVector4 SkinnedPosition(0, 0, 0, 0);
        FVector4 SkinnedNormal(0, 0, 0, 0);
        FVector4 SkinnedTangent(0, 0, 0, 0);

        for (int i = 0; i < 8; ++i)
        {
            if (Vertex.BoneWeights[i] > 0.0f)
            {
                //const FMatrix& BoneMatrix = RenderData->InverseBindPoseMatrices[Vertex.BoneIndices[i]];
                //const FMatrix& BoneMatrix = BoneMatrices[Vertex.BoneIndices[i]];

                const FMatrix& BoneMatrix = FMatrix::Identity;
                SkinnedPosition += BoneMatrix.TransformPosition(Vertex.Position) * Vertex.BoneWeights[i];
                SkinnedNormal += BoneMatrix.TransformFVector4(FVector4(Vertex.Normal, 0)).xyz() * Vertex.BoneWeights[i];
                SkinnedTangent += BoneMatrix.TransformFVector4(FVector4(Vertex.Tangent, Vertex.TangentW)).xyz() * Vertex.BoneWeights[i];
            }
        }
        FSkeletalMeshVertex NewVertex = Vertex;
        NewVertex.Position = SkinnedPosition.Pos();
        NewVertex.Normal = SkinnedNormal.Pos();
        NewVertex.Tangent = SkinnedTangent.xyz();
        NewVertex.TangentW = SkinnedTangent.W;

        NewVertices.Add(NewVertex);
    }

    BufferManager->UpdateDynamicVertexBuffer(RenderData->ObjectName, NewVertices);
}


void FSkeletalMeshRenderPass::ClearRenderArr()
{
    SkeletalMeshComponents.Empty();
}
