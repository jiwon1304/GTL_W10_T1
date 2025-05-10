#pragma once
#include "Define.h"
#include "Hal/PlatformType.h"
#include "Container/Array.h"
#include "Container/Map.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Color.h"
#include "Math/Transform.h"

struct FMeshBoneInfo
{
    FString Name;
    int32 ParentIndex;
};

// inverse bind pose는 USkeletalMesh에 존재.
struct FReferenceSkeleton
{
    // TODO : RawRefBonePose을 행렬로 캐싱하기
    TArray<FMeshBoneInfo> RawRefBoneInfo;
    // joint pose 저장용도. Index는 RawRefBoneInfo를 따라갑니다.
    TArray<FTransform> RawRefBonePose;
    TMap<FString, int32> RawNameToIndexMap;

    int32 GetRawBoneNum() const
    {
        return RawRefBoneInfo.Num();
    }
};  

// !!! FFbxVertex랑 메모리 레이아웃이 같아야합니다.
struct FSkeletalVertex
{
    FVector Position;
    FLinearColor Color;
    FVector Normal;
    FVector4 Tangent;
    FVector2D UV;
    int MaterialIndex;
    int BoneIndices[8];
    float BoneWeights[8];

    inline const static D3D11_INPUT_ELEMENT_DESC LayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"MATERIAL_INDEX", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BONE_INDICES", 1, DXGI_FORMAT_R32G32B32A32_SINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BONE_WEIGHTS", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    }; 
};

struct FSkelMeshRenderSection
{
    TArray<FSkeletalVertex> Vertices;
    TArray<uint32> Indices;
    TArray<uint32> SubsetIndex;
    //TArray<uint8> BoneIndices;
    //TArray<float> BoneWeights;
    FString Name;
};

// 수정되지 않는 데이터입니다.
// 포즈를 수정하려면 USkeletalMesh에 있는 RefSkeleton을 수정해야합니다.
struct FSkeletalMeshRenderData
{
    FString ObjectName;
    //FString DisplayName;

    // Render Data
    //TArray<FVector> Vertices;
    //TArray<FLinearColor> Colors;
    //TArray<FVector> Normals;
    //TArray<FVector> Tangents;
    //TArray<FVector2D> UVs;
    //TArray<uint32> MaterialIndices;
    //TArray<uint8> BoneIndices;
    //TArray<float> BoneWeights;

    TArray<FSkelMeshRenderSection> RenderSections;
    TArray<FMaterialSubset> MaterialSubsets;
};
