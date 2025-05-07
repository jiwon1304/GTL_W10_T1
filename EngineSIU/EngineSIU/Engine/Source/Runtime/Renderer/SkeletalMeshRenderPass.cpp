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
    for (const auto& Component : TObjectRange<USkeletalMeshComponent>())
    {
        SkeletalMeshComponents.Add(Component);
    }
}

void FSkeletalMeshRenderPass::PrepareRenderState(const std::shared_ptr<FEditorViewportClient> Viewport)
{
    const EViewModeIndex ViewMode = Viewport->GetViewMode();

    ChangeViewMode(ViewMode);

    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    TArray<FString> PSBufferKeys = {
        TEXT("FLightInfoBuffer"),
        TEXT("FMaterialConstants"),
        TEXT("FLitUnlitConstants"),
        TEXT("FSubMeshConstants"),
        TEXT("FTextureConstants"),
    };

    BufferManager->BindConstantBuffers(PSBufferKeys, 0, EShaderStage::Pixel);
    BufferManager->BindConstantBuffer(TEXT("FDiffuseMultiplier"), 6, EShaderStage::Pixel);

    BufferManager->BindConstantBuffer(TEXT("FLightInfoBuffer"), 0, EShaderStage::Vertex);
    BufferManager->BindConstantBuffer(TEXT("FMaterialConstants"), 1, EShaderStage::Vertex);
    BufferManager->BindConstantBuffer(TEXT("FObjectConstantBuffer"), 12, EShaderStage::Vertex);
    BufferManager->BindConstantBuffer("FBoneMatrices", 11, EShaderStage::Vertex);

    Graphics->DeviceContext->RSSetViewports(1, &Viewport->GetViewportResource()->GetD3DViewport());

    const EResourceType ResourceType = EResourceType::ERT_Scene;
    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    FRenderTargetRHI* RenderTargetRHI = ViewportResource->GetRenderTarget(ResourceType);
    FDepthStencilRHI* DepthStencilRHI = ViewportResource->GetDepthStencil(ResourceType);

    Graphics->DeviceContext->OMSetRenderTargets(1, &RenderTargetRHI->RTV, DepthStencilRHI->DSV); 
}

void FSkeletalMeshRenderPass::ChangeViewMode(EViewModeIndex ViewMode)
{
    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;
    
    switch (ViewMode)
    {
    case EViewModeIndex::VMI_Lit_Gouraud:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"GOURAUD_SkeletalMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"GOURAUD_SkeletalMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"GOURAUD_StaticMeshPixelShader");
        UpdateLitUnlitConstant(1);
        break;
    case EViewModeIndex::VMI_Lit_Lambert:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkeletalMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkeletalMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"LAMBERT_StaticMeshPixelShader");
        UpdateLitUnlitConstant(1);
        break;
    case EViewModeIndex::VMI_Lit_BlinnPhong:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkeletalMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkeletalMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
        UpdateLitUnlitConstant(1);
        break;
    case EViewModeIndex::VMI_LIT_PBR:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkeletalMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkeletalMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"PBR_StaticMeshPixelShader");
        UpdateLitUnlitConstant(1);
        break;
    case EViewModeIndex::VMI_Wireframe:
    case EViewModeIndex::VMI_Unlit:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkeletalMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkeletalMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"LAMBERT_StaticMeshPixelShader");
        UpdateLitUnlitConstant(0);
        break;
    case EViewModeIndex::VMI_SceneDepth:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkeletalMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkeletalMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderDepth");
        UpdateLitUnlitConstant(0);
        break;
    case EViewModeIndex::VMI_WorldNormal:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkeletalMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkeletalMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderWorldNormal");
        UpdateLitUnlitConstant(0);
        break;
    case EViewModeIndex::VMI_WorldTangent:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkeletalMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkeletalMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderWorldTangent");
        UpdateLitUnlitConstant(0);
        break;
    // HeatMap ViewMode 등
    default:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkeletalMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkeletalMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"LAMBERT_StaticMeshPixelShader");
        UpdateLitUnlitConstant(1);
        break;
    }

    // Rasterizer
    Graphics->ChangeRasterizer(ViewMode);

    // Setup
    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);
    Graphics->DeviceContext->PSSetShader(PixelShader, nullptr, 0);
}

void FSkeletalMeshRenderPass::UpdateLitUnlitConstant(int32 isLit) const
{
    FLitUnlitConstants Data;
    Data.bIsLit = isLit;
    BufferManager->UpdateConstantBuffer(TEXT("FLitUnlitConstants"), Data);
}

void FSkeletalMeshRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{

    PrepareRenderState(Viewport);

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

        // FSkeletalMeshRenderData* RenderData = SkinnedMeshData->GetSkeletalMesh()->GetRenderData();
        USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
        if (!SkeletalMesh) continue;
        FSkeletalMeshRenderData* Renderdata = SkeletalMesh->GetRenderData();
        if (!Renderdata) continue;

        // Bone Matrix는 CPU에서 처리
        // Model -> j -> transform -> model space로 변환하는 행렬
        // 즉, transform을 적용해주는 행렬
        TArray<FMatrix> SkinningMatrices;
        SkeletalMeshComponent->GetSkinningMatrices(SkinningMatrices);

        // Update constant buffers
        UpdateObjectConstant(
            SkeletalMeshComponent->GetWorldMatrix(),
            SkeletalMeshComponent->EncodeUUID() / 255.0f,
            SkeletalMeshComponent->IsActive(),
            bIsCPUSkinning
        );

        TArray<UMaterial*> Materials;
        SkeletalMesh->GetUsedMaterials(Materials);
        TArray<UMaterial*> OverrideMaterials = SkeletalMeshComponent->GetOverrideMaterials();

        for (int SectionIndex = 0; SectionIndex < Renderdata->RenderSections.Num(); ++SectionIndex)
        {
            const FSkelMeshRenderSection& RenderSection = Renderdata->RenderSections[SectionIndex];
            FVertexInfo VertexInfo;
            if (SkeletalMesh->bCPUSkinned)
            {
                // Update vertex buffer
                TArray<FSkeletalVertex> Vertices;
                GetSkinnedVertices(SkeletalMesh, SectionIndex, SkinningMatrices, Vertices);

                BufferManager->CreateDynamicVertexBuffer(RenderSection.Name, Vertices, VertexInfo);
                BufferManager->UpdateDynamicVertexBuffer(RenderSection.Name, Vertices);
            }
            else
            {
                // Update bone matrices
                UpdateBoneMatrices(SkinningMatrices);
                BufferManager->CreateVertexBuffer(RenderSection.Name,
                    RenderSection.Vertices, VertexInfo);
            }

            Graphics->DeviceContext->IASetVertexBuffers(0, 1, &VertexInfo.VertexBuffer, &VertexInfo.Stride, &VertexInfo.Offset);

            FIndexInfo IndexInfo;
            BufferManager->CreateIndexBuffer(RenderSection.Name, RenderSection.Indices, IndexInfo);
            if (IndexInfo.IndexBuffer)
            {
                Graphics->DeviceContext->IASetIndexBuffer(IndexInfo.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
            }

            for (int i = 0; i < RenderSection.SubsetIndex.Num(); ++i)
            {
                uint32 SubsetIndex = RenderSection.SubsetIndex[i];
                uint32 MaterialIndex = Renderdata->MaterialSubsets[SubsetIndex].MaterialIndex;

                if (MaterialIndex < OverrideMaterials.Num() && OverrideMaterials[MaterialIndex] != nullptr)
                {
                    MaterialUtils::UpdateMaterial(BufferManager, Graphics, OverrideMaterials[MaterialIndex]->GetMaterialInfo());
                }
                else
                {
                    MaterialUtils::UpdateMaterial(BufferManager, Graphics, Materials[MaterialIndex]->GetMaterialInfo());
                }

                uint32 StartIndex = Renderdata->MaterialSubsets[SubsetIndex].IndexStart;
                uint32 IndexCount = Renderdata->MaterialSubsets[SubsetIndex].IndexCount;
                Graphics->DeviceContext->DrawIndexed(IndexCount, StartIndex, 0);
            }
        }
    }


        /////////////////////////////////////////////////


    //    for (int i = 0; i < SkeletalMesh->GetRenderData()->RenderSections.Num(); ++i)
    //    {
    //        FFbxMeshData& MeshData = SkeletalMesh->mesh[i];
    //        
    //        FVertexInfo VertexInfo;
    //        if (bIsCPUSkinning)
    //        {
    //            BufferManager->CreateDynamicVertexBuffer(MeshData.name, MeshData.vertices, VertexInfo);
    //            UpdateVertexBuffer(SkeletalMesh->mesh[i], SkinningMatrices);
    //        }
    //        else
    //        {
    //            BufferManager->CreateVertexBuffer(MeshData.name, MeshData.vertices, VertexInfo);
    //            UpdateBoneMatrices(SkinningMatrices);
    //        }

    //        Graphics->DeviceContext->IASetVertexBuffers(0, 1, &VertexInfo.VertexBuffer, &VertexInfo.Stride, &VertexInfo.Offset);

    //        FIndexInfo IndexInfo;
    //        BufferManager->CreateIndexBuffer(MeshData.name, MeshData.indices, IndexInfo);
    //        if (IndexInfo.IndexBuffer)
    //        {
    //            Graphics->DeviceContext->IASetIndexBuffer(IndexInfo.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    //        }

    //    
    //        // Graphics->DeviceContext->DrawIndexed(IndexInfo.NumIndices, 0, 0);

    //        for (int i = 0; i < MeshData.subsetIndex.Num(); ++i)
    //        {
    //            uint32 SubsetIndex = MeshData.subsetIndex[i];
    //            uint32 MaterialIndex = SkeletalMesh->materialSubsets[SubsetIndex].MaterialIndex;
    //        
    //            if (MaterialIndex < OverrideMaterials.Num() && OverrideMaterials[MaterialIndex] != nullptr)
    //            {
    //                MaterialUtils::UpdateMaterial(BufferManager, Graphics, OverrideMaterials[MaterialIndex]->GetMaterialInfo());
    //            }
    //            else
    //            {
    //                MaterialUtils::UpdateMaterial(BufferManager, Graphics, Materials[MaterialIndex]->GetMaterialInfo());
    //            }
    //        
    //            uint32 StartIndex = SkeletalMesh->materialSubsets[SubsetIndex].IndexStart;
    //            uint32 IndexCount = SkeletalMesh->materialSubsets[SubsetIndex].IndexCount;
    //            Graphics->DeviceContext->DrawIndexed(IndexCount, StartIndex, 0);
    //        }
    //    }

    //}
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
    HRESULT hr = ShaderManager->AddVertexShaderAndInputLayout(L"SkeletalMeshVertexShader", L"Shaders/SkeletalMeshVertexShader.hlsl", "mainVS", FSkeletalVertex::LayoutDesc, ARRAYSIZE(FSkeletalVertex::LayoutDesc));
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

void FSkeletalMeshRenderPass::GetSkinnedVertices(USkeletalMesh* SkeletalMesh, uint32 Section, const TArray<FMatrix>& BoneMatrices, TArray<FSkeletalVertex>& OutVertices) const
{
    const FSkelMeshRenderSection& RenderSection = SkeletalMesh->GetRenderData()->RenderSections[Section];
    const TArray<FSkeletalVertex>& Vertices = RenderSection.Vertices;
    OutVertices.SetNum(Vertices.Num());
    for (int i = 0; i < Vertices.Num(); ++i)
    {
        const FSkeletalVertex& Vertex = Vertices[i];
        FSkeletalVertex& SkinnedVertex = OutVertices[i];
        FVector4 SkinnedPosition(0, 0, 0, 0);
        FVector4 SkinnedNormal(0, 0, 0, 0);
        FVector4 SkinnedTangent(0, 0, 0, 0);
        for (int j = 0; j < 8; ++j)
        {
            if (Vertex.BoneWeights[j] > 0.0f)
            {
                const FMatrix& BoneMatrix = BoneMatrices[Vertex.BoneIndices[j]];
                SkinnedPosition += BoneMatrix.TransformPosition(Vertex.Position) * Vertex.BoneWeights[j];
                SkinnedNormal += BoneMatrix.TransformFVector4(FVector4(Vertex.Normal, 0)) * Vertex.BoneWeights[j];
                SkinnedTangent += BoneMatrix.TransformFVector4(Vertex.Tangent) * Vertex.BoneWeights[j];
            }
        }
        SkinnedVertex.Position = SkinnedPosition.Pos();
        SkinnedVertex.Normal = SkinnedNormal.Pos();
        SkinnedVertex.Tangent = SkinnedTangent.Pos();
    }
}


void FSkeletalMeshRenderPass::ClearRenderArr()
{
    SkeletalMeshComponents.Empty();
}
