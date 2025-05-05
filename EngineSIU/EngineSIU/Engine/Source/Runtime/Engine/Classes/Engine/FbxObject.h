#pragma once
#include "Define.h"

struct FFbxVertex
{
    FVector vertex;
    FVector normal;
    FVector tangent;
    FVector2D uv;
    int materialIndex = -1;
    int boneIndices[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    float boneWeights[8];
};
struct FFbxMeshData
{
    TArray<FFbxVertex> vertices;
    TArray<uint32> indices;
    FString name;
};

struct FFbxJoint
{
    FString name;
    int parentIndex;
    FMatrix localBindPose;
    FMatrix inverseBindPose;
};

struct FFbxSkeletonData
{
    TArray<FFbxJoint> joints;
};

struct FFbxMaterialPhong
{
    FTextureInfo normalMapInfo;
    FVector diffuseColor;
    FTextureInfo diffuseMapInfo;
    FVector ambientColor;
    FTextureInfo ambientMapInfo;
    FVector specularColor;
    FTextureInfo specularMapInfo;
    FVector emissiveColor;
    FTextureInfo emissiveMapInfo;
    float shininessColor;
    float transparencyFactor;
};

struct FSkinnedMesh {
    FFbxMeshData mesh;
    FFbxSkeletonData skeleton;
    TArray<FFbxMaterialPhong> material;
    FVector AABBmin, AABBmax;
};
