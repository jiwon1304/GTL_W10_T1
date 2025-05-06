#include "SkinnedMeshRenderPass.h"

#include "UnrealClient.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "UnrealEd/EditorViewportClient.h"
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
    CreateShader();
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
    PrepareRenderState(Viewport);
    RenderAllSkinnedMeshes();
}

void FSkinnedMeshRenderPass::ClearRenderArr()
{
    SkinnedMeshComponents.Empty();
}

void FSkinnedMeshRenderPass::CreateShader() const
{
    constexpr D3D11_INPUT_ELEMENT_DESC LayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"MATERIAL_INDEX", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    }; 

    HRESULT Result = ShaderManager->AddVertexShaderAndInputLayout(
        L"SkinnedMeshVertexShader",
        L"Shaders/SkinnedMeshVertexShader.hlsl",
        "mainVS",
        LayoutDesc,
        ARRAYSIZE(LayoutDesc)
    );

    if (FAILED(Result))
    {
        UE_LOG(ELogLevel::Error, L"Failed to create vertex shader: %s", L"SkinnedMeshVertexShader");
    }
}

void FSkinnedMeshRenderPass::PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport) const
{
    const EViewModeIndex ViewMode = Viewport->GetViewMode();

    ChangeViewMode(ViewMode);

    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const TArray<FString> PSBufferKeys = {
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

    constexpr EResourceType ResourceType = EResourceType::ERT_Scene;
    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    FRenderTargetRHI* RenderTargetRHI = ViewportResource->GetRenderTarget(ResourceType);
    FDepthStencilRHI* DepthStencilRHI = ViewportResource->GetDepthStencil(ResourceType);

    Graphics->DeviceContext->OMSetRenderTargets(1, &RenderTargetRHI->RTV, DepthStencilRHI->DSV); 
}

void FSkinnedMeshRenderPass::ChangeViewMode(EViewModeIndex ViewMode) const
{
    ID3D11VertexShader* VertexShader;
    ID3D11InputLayout* InputLayout;
    ID3D11PixelShader* PixelShader;
    
    switch (ViewMode)  // NOLINT(clang-diagnostic-switch-enum)
    {
    case EViewModeIndex::VMI_Lit_Gouraud:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"GOURAUD_SkinnedMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"GOURAUD_SkinnedMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"GOURAUD_StaticMeshPixelShader");
        UpdateLitUnlitConstant(true);
        break;
    case EViewModeIndex::VMI_Lit_Lambert:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkinnedMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkinnedMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"LAMBERT_StaticMeshPixelShader");
        UpdateLitUnlitConstant(true);
        break;
    case EViewModeIndex::VMI_Lit_BlinnPhong:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkinnedMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkinnedMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
        UpdateLitUnlitConstant(true);
        break;
    case EViewModeIndex::VMI_LIT_PBR:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkinnedMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkinnedMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"PBR_StaticMeshPixelShader");
        UpdateLitUnlitConstant(true);
        break;
    case EViewModeIndex::VMI_Wireframe:
    case EViewModeIndex::VMI_Unlit:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkinnedMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkinnedMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"LAMBERT_StaticMeshPixelShader");
        UpdateLitUnlitConstant(false);
        break;
    case EViewModeIndex::VMI_SceneDepth:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkinnedMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkinnedMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderDepth");
        UpdateLitUnlitConstant(false);
        break;
    case EViewModeIndex::VMI_WorldNormal:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkinnedMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkinnedMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderWorldNormal");
        UpdateLitUnlitConstant(false);
        break;
    case EViewModeIndex::VMI_WorldTangent:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkinnedMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkinnedMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderWorldTangent");
        UpdateLitUnlitConstant(false);
        break;
    // HeatMap ViewMode 등
    default:
        VertexShader = ShaderManager->GetVertexShaderByKey(L"SkinnedMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"SkinnedMeshVertexShader");
        PixelShader = ShaderManager->GetPixelShaderByKey(L"LAMBERT_StaticMeshPixelShader");
        UpdateLitUnlitConstant(true);
        break;
    }

    // Rasterizer
    Graphics->ChangeRasterizer(ViewMode);

    // Setup
    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);
    Graphics->DeviceContext->PSSetShader(PixelShader, nullptr, 0);
}

void FSkinnedMeshRenderPass::UpdateLitUnlitConstant(bool bIsLit) const
{
    FLitUnlitConstants Data;
    Data.bIsLit = static_cast<int32>(bIsLit);
    BufferManager->UpdateConstantBuffer(TEXT("FLitUnlitConstants"), Data);
}

void FSkinnedMeshRenderPass::RenderAllSkinnedMeshes()
{
    for (USkinnedMeshComponent* SkinnedMeshComponent : SkinnedMeshComponents)
    {
        if (!SkinnedMeshComponent || !SkinnedMeshComponent->GetSkeletalMesh())
        {
            continue;
        }

        USkeletalMesh* SkeletalMesh = SkinnedMeshComponent->GetSkeletalMesh();
        if (!SkeletalMesh) continue;

        // Bone Matrix는 CPU에서 처리
        TArray<FMatrix> BoneMatrices = SkinnedMeshComponent->CalculateBoneMatrices();

        // Update constant buffers
        UpdateObjectConstant(
            SkinnedMeshComponent->GetWorldMatrix(), 
            SkinnedMeshComponent->EncodeUUID() / 255.0f,
            SkinnedMeshComponent->IsActive(),
            bIsCPUSkinning
        );

        // TArray<UMaterial*> Materials = SkinnedMeshComponent->GetSkinnedMesh()->material;
        // TArray<UMaterial*> OverrideMaterials = SkinnedMeshComponent->GetOverrideMaterials();

        FString KeyName = SkeletalMesh->Info.AssetName.ToString();

        FVertexInfo VertexInfo;
        if (bIsCPUSkinning)
        {
            BufferManager->CreateDynamicVertexBuffer(KeyName, SkeletalMesh->Vertices, VertexInfo);
            UpdateVertexBuffer(*SkeletalMesh, BoneMatrices);
        }
        else
        {
            BufferManager->CreateVertexBuffer(KeyName, SkeletalMesh->Vertices, VertexInfo);
            UpdateBoneMatrices(BoneMatrices);
        }

        Graphics->DeviceContext->IASetVertexBuffers(0, 1, &VertexInfo.VertexBuffer, &VertexInfo.Stride, &VertexInfo.Offset);

        FIndexInfo IndexInfo;
        BufferManager->CreateIndexBuffer(KeyName, SkeletalMesh->Indices, IndexInfo);
        if (IndexInfo.IndexBuffer)
        {
            Graphics->DeviceContext->IASetIndexBuffer(IndexInfo.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        }

        Graphics->DeviceContext->DrawIndexed(IndexInfo.NumIndices, 0, 0);
    }
}

void FSkinnedMeshRenderPass::UpdateVertexBuffer(const USkeletalMesh& MeshData, const TArray<FMatrix>& BoneMatrices) const
{
    TArray<FSkinnedVertex> NewVertices;
    NewVertices.Reserve(MeshData.Vertices.Num());

    static auto PerspectiveDivide = [](const FVector4& InVector) -> FVector
    {
        if (InVector.W != 0.0f)
        {
            return FVector{InVector.X / InVector.W, InVector.Y / InVector.W, InVector.Z / InVector.W};
        }
        return FVector{InVector.X, InVector.Y, InVector.Z};
    };

    for (const FSkinnedVertex& Vertex : MeshData.Vertices)
    {
        FVector4 SkinnedPosition(0, 0, 0, 0);
        FVector4 SkinnedNormal(0, 0, 0, 0);
        FVector4 SkinnedTangent(0, 0, 0, 0);

        for (int Idx = 0; Idx < 4; ++Idx)
        {
            if (Vertex.BoneWeights[Idx] > 0.0f)
            {
                const FMatrix& BoneMatrix = BoneMatrices[Vertex.BoneIndices[Idx]];

                SkinnedPosition += BoneMatrix.TransformPosition(Vertex.Position) * Vertex.BoneWeights[Idx];
                SkinnedNormal += BoneMatrix.TransformFVector4(FVector4(Vertex.Normal, 0)) * Vertex.BoneWeights[Idx];
                SkinnedTangent += BoneMatrix.TransformFVector4(Vertex.Tangent) * Vertex.BoneWeights[Idx];
            }
        }
        FSkinnedVertex NewVertex = Vertex;
        NewVertex.Position = PerspectiveDivide(SkinnedPosition);
        NewVertex.Normal = PerspectiveDivide(SkinnedNormal);
        NewVertex.Tangent = PerspectiveDivide(SkinnedTangent);

        NewVertices.Add(NewVertex);
    }

    BufferManager->UpdateDynamicVertexBuffer(MeshData.Info.AssetName.ToString(), NewVertices);
}

void FSkinnedMeshRenderPass::UpdateObjectConstant(const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected, bool bCPUSkinning) const
{
    FObjectConstantBuffer ObjectData = {};
    ObjectData.WorldMatrix = WorldMatrix;
    ObjectData.InverseTransposedWorld = FMatrix::Transpose(FMatrix::Inverse(WorldMatrix));
    ObjectData.UUIDColor = UUIDColor;
    ObjectData.bIsSelected = bIsSelected;
    ObjectData.bCPUSkinning = bCPUSkinning ? 1 : 0;

    BufferManager->UpdateConstantBuffer(TEXT("FObjectConstantBuffer"), ObjectData);
}

void FSkinnedMeshRenderPass::UpdateBoneMatrices(const TArray<FMatrix>& BoneMatrices) const
{
    BufferManager->UpdateConstantBuffer(TEXT("FBoneMatrices"), BoneMatrices);
}
