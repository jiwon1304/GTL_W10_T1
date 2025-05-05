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
#include "Components/SkinnedMeshComponent.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "Engine/EditorEngine.h"
#include "Engine/FbxObject.h"
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
    for (const auto& Component : TObjectRange<USkinnedMeshComponent>())
    {
        SkinnedMeshComponent.Add(Component);
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
    for (USkinnedMeshComponent* SkinnedMeshData : SkinnedMeshComponent)
    {
        if (!SkinnedMeshData) continue;
        if (!SkinnedMeshData->GetSkinnedMesh()) continue;

        // FSkeletalMeshRenderData* RenderData = SkinnedMeshData->GetSkeletalMesh()->GetRenderData();
        FSkinnedMesh* RenderData = SkinnedMeshData->GetSkinnedMesh();
        if (!RenderData) continue;

        // Bone Matrix는 CPU에서 처리
        TArray<FMatrix> BoneMatrices;
        SkinnedMeshData->CalculateBoneMatrices(BoneMatrices);

        // Update constant buffers
        UpdateObjectConstant(
            SkinnedMeshData->GetWorldMatrix(), 
            SkinnedMeshData->EncodeUUID() / 255.0f,
            SkinnedMeshData->IsActive(),
            bIsCPUSkinning
        );

        TArray<UMaterial*> Materials = SkinnedMeshData->GetSkinnedMesh()->material;
        TArray<UMaterial*> OverrideMaterials = SkinnedMeshData->GetOverrideMaterials();

        for (int i = 0; i < RenderData->mesh.Num(); ++i)
        {
            FFbxMeshData& MeshData = RenderData->mesh[i];
            
            FVertexInfo VertexInfo;
            if (bIsCPUSkinning)
            {
                BufferManager->CreateDynamicVertexBuffer(MeshData.name, MeshData.vertices, VertexInfo);
                UpdateVertexBuffer(RenderData->mesh[i], BoneMatrices);
            }
            else
            {
                BufferManager->CreateVertexBuffer(MeshData.name, MeshData.vertices, VertexInfo);
                UpdateBoneMatrices(BoneMatrices);
            }

            Graphics->DeviceContext->IASetVertexBuffers(0, 1, &VertexInfo.VertexBuffer, &VertexInfo.Stride, &VertexInfo.Offset);

            FIndexInfo IndexInfo;
            BufferManager->CreateIndexBuffer(MeshData.name, MeshData.indices, IndexInfo);
            if (IndexInfo.IndexBuffer)
            {
                Graphics->DeviceContext->IASetIndexBuffer(IndexInfo.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
            }

        
            Graphics->DeviceContext->DrawIndexed(IndexInfo.NumIndices, 0, 0);

            // TODO: parse material subset from fbx
            // for (int SubMeshIndex = 0; SubMeshIndex < RenderData->material.Num(); SubMeshIndex++)
            // {
            //     uint32 MaterialIndex = RenderData->material[SubMeshIndex].MaterialIndex;
            //
            //     if (MaterialIndex < OverrideMaterials.Num() && OverrideMaterials[MaterialIndex] != nullptr)
            //     {
            //         MaterialUtils::UpdateMaterial(BufferManager, Graphics, OverrideMaterials[MaterialIndex]->GetMaterialInfo());
            //     }
            //     else
            //     {
            //         MaterialUtils::UpdateMaterial(BufferManager, Graphics, Materials[MaterialIndex]->Material->GetMaterialInfo());
            //     }
            //
            //     uint32 StartIndex = RenderData->MaterialSubsets[SubMeshIndex].IndexStart;
            //     uint32 IndexCount = RenderData->MaterialSubsets[SubMeshIndex].IndexCount;
            //     Graphics->DeviceContext->DrawIndexed(IndexCount, StartIndex, 0);
            // }
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
    HRESULT hr = ShaderManager->AddVertexShaderAndInputLayout(L"SkeletalMeshVertexShader", L"Shaders/SkeletalMeshVertexShader.hlsl", "mainVS", FFbxVertex::LayoutDesc, ARRAYSIZE(FFbxVertex::LayoutDesc));
    if (FAILED(hr))
    {
        return;
    }
}

void FSkeletalMeshRenderPass::UpdateVertexBuffer(FFbxMeshData& meshData, const TArray<FMatrix>& BoneMatrices)
{
    TArray<FFbxVertex> NewVertices;
    NewVertices.Reserve(meshData.vertices.Num());

    for (const FFbxVertex& Vertex : meshData.vertices)
    {
        FVector4 SkinnedPosition(0, 0, 0, 0);
        FVector4 SkinnedNormal(0, 0, 0, 0);
        FVector4 SkinnedTangent(0, 0, 0, 0);

        for (int i = 0; i < 8; ++i)
        {
            if (Vertex.boneWeights[i] > 0.0f)
            {
                const FMatrix& BoneMatrix = BoneMatrices[Vertex.boneIndices[i]];

                SkinnedPosition += BoneMatrix.TransformPosition(Vertex.position) * Vertex.boneWeights[i];
                SkinnedNormal += BoneMatrix.TransformFVector4(FVector4(Vertex.normal, 0)) * Vertex.boneWeights[i];
                SkinnedTangent += BoneMatrix.TransformFVector4(Vertex.tangent) * Vertex.boneWeights[i];
            }
        }
        FFbxVertex NewVertex = Vertex;
        NewVertex.position = SkinnedPosition.Pos();
        NewVertex.normal = SkinnedNormal.Pos();
        NewVertex.tangent = SkinnedTangent.Pos();

        NewVertices.Add(NewVertex);
    }

    BufferManager->UpdateDynamicVertexBuffer(meshData.name, NewVertices);
}


void FSkeletalMeshRenderPass::ClearRenderArr()
{
    SkinnedMeshComponent.Empty();
}
