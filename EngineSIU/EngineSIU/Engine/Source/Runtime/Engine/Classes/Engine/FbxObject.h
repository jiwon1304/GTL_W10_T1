#pragma once
#include "Define.h"

struct FFbxVertex
{
    FVector vertex;
    FVector normal;
    FVector tangent;
    FVector2D uv;
    uint32 materialIndex;
    uint8 boneIndices[4] = { 255, 255, 255, 255 };
    float boneWeights[4];
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

struct FFbxObject {
    FFbxMeshData mesh;
    FFbxSkeletonData skeleton;
    TArray<FFbxMaterialPhong> material;
};
