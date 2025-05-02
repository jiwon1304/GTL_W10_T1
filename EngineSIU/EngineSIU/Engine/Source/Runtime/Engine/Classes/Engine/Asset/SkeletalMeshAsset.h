#pragma once
#include "Define.h"
#include "Hal/PlatformType.h"
#include "Container/Array.h"

struct FSkeletalMeshVertex
{
    FVector Position;       // Vertex Position (x, y, z)
    FVector Normal;         // Normal (nx, ny, nz)
    FVector Tangent;        // Tangent (tx, ty, tz)
    float TangentW;         // Tangent W for Handedness
    FVector2D UV;           // UV Coordinates (u, v)
    int BoneIndices[4];     // Indices of Skinning Bones
    float BoneWeights[4];   // Weights for Bone Influence
    uint32 MaterialIndex;   // Material Index
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
};
