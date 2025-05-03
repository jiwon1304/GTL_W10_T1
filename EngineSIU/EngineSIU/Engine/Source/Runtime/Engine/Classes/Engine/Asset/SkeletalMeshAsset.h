#pragma once
#include "Define.h"
#include "Hal/PlatformType.h"
#include "Container/Array.h"

struct FSkeletalMeshVertex
{
    FVector Position;       // Vertex Position (x, y, z)
    FVector4 Color;
    FVector Normal;         // Normal (nx, ny, nz)
    FVector Tangent;        // Tangent (tx, ty, tz)
    float TangentW;         // Tangent W for Handedness
    FVector2D UV;           // UV Coordinates (u, v)
    uint32 MaterialIndex;   // Material Index
    int BoneIndices[4];     // Indices of Skinning Bones
    float BoneWeights[4];   // Weights for Bone Influence
};

struct FSkeletalMeshBone
{
    FString Name;                  // Name of the Bone
    int ParentIndex;               // Parent Bone Index (-1 if root)
    FMatrix BindPoseMatrix;        // Bind Pose Transform (Matrix)
};


struct FSkeletalMeshRenderData
{
    FWString ObjectName;            // Object Name
    FString DisplayName;           // Display Name of the Mesh
    TArray<FSkeletalMeshVertex> Vertices; // Vertices with Skinning Data
    TArray<uint32> Indices;        // Indices
    TArray<FSkeletalMeshBone> Bones; // Bone Hierarchy
    TArray<FMatrix> InverseBindPoseMatrices; // Inverse Bind Pose Matrices

    TArray<FMaterialInfo> Materials;
    TArray<FMaterialSubset> MaterialSubsets;

    inline const static D3D11_INPUT_ELEMENT_DESC LayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"MATERIAL_INDEX", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

};
