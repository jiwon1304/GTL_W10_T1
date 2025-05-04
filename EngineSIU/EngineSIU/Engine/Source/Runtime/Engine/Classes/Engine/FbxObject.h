#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

struct FFbxMeshData
{
    TArray<FVector> vertices;
    TArray<FVector> normals;
    TArray<FVector2D> uvs;
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
