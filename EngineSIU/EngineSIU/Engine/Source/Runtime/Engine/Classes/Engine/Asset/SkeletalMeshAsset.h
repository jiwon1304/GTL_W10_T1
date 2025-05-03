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
    // Bone Space -> Model Space로의 변환행렬
    // BindPoseMatrix는 FBX에서 가져온 Matrix를 그대로 사용합니다.
    FMatrix BindPoseMatrix;
};


struct FSkeletalMeshRenderData
{
    FWString ObjectName;            // FBX파일의 이름(저장, indexing에 이용됨)
    FString DisplayName;           // 보여질 이름

    // FBX파일이 가지고 있는 Vertex 정보
    // Vertex는 Model Space 기준입니다.
    TArray<FSkeletalMeshVertex> Vertices;
    TArray<int32> Indices;        // FBX파일이 가지고 있는 Index 정보

    //Bone의 이름, Parent Index, BindPoseMatrix를 가집니다. 
    //구조체 FSkeletalMeshBone이 직접 index를 가지진 않습니다. (TArray의 index를 사용)
    TArray<FSkeletalMeshBone> Bones; 

    // Model Space -> Bone Space로의 변환행렬
    // 애니메이션을 하려면, Model Space에 있는 Vertex를 Bone Space로 변환해야 합니다.
    // 이후 Bone Space에서 애니메이션을 하고, 다시 Model Space로 변환합니다.
    // 이 과정에서 InverseBindPoseMatrix가 필요합니다.
    TArray<FMatrix> InverseBindPoseMatrices;

    TArray<FMaterialInfo> Materials;
    // Material당 Index를 나눠놓은 구조체로, Material이 없으면 기본 Material에 모든 Index가 들어갑니다.
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
