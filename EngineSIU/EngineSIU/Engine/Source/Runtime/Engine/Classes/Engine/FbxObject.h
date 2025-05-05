#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

struct FFbxVertex
{
    FVector vertex;
    FVector normal;
    FVector2D uv;
    uint8 boneIndices[4] = { 255, 255, 255, 255 };
    float boneWeights[4];
};
struct FFbxMeshData
{
    // TArray<FVector> vertices;
    // TArray<FVector> normals;
    // TArray<FVector2D> uvs;
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


struct FFbxObject {
    FFbxMeshData mesh;
    FFbxSkeletonData skeleton;
};
