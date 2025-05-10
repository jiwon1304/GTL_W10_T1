#pragma once
#include "Define.h"
#include "Math/Quat.h"

// !!! 파싱할때의 자료구조로, 파서 이외에서는 사용하지 않습니다.
struct FFbxVertex
{
    FVector position;
    FLinearColor color;
    FVector normal;
    FVector4 tangent;
    FVector2D uv;
    int materialIndex = -1;
    int boneIndices[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    float boneWeights[8] = { 0,0,0,0,0,0,0,0 };
    //inline const static D3D11_INPUT_ELEMENT_DESC LayoutDesc[] = {
    //    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    //    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    //    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    //    {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    //    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    //    {"MATERIAL_INDEX", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    //    {"BONE_INDICES", 0, DXGI_FORMAT_R8G8B8A8_SINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    //    {"BONE_INDICES", 1, DXGI_FORMAT_R8G8B8A8_SINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    //    {"BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    //    {"BONE_WEIGHTS", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    //}; 
};
struct FFbxMeshData
{
    TArray<FFbxVertex> vertices;
    TArray<uint32> indices;
    TArray<uint32> subsetIndex;
    FString name;
};

struct FFbxJoint
{
    FString name;
    int parentIndex;
    FMatrix localBindPose;
    FMatrix inverseBindPose;
    FVector position;
    FQuat rotation;
    FVector scale;
};

struct FFbxSkeletonData
{
    TArray<FFbxJoint> joints;
};

struct FFbxSkeletalMesh {
    FString name;
    TArray<FFbxMeshData> mesh;
    FFbxSkeletonData skeleton;
    TArray<UMaterial*> material;
    TArray<FMaterialSubset> materialSubsets;
    FVector AABBmin, AABBmax;
};
