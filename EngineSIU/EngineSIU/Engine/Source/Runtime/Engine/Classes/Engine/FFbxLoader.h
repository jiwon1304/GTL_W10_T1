#pragma once
#include <fbxsdk.h>

#include "FbxObject.h"
#include "Container/Array.h"
#include "Container/Map.h"
#include "Container/String.h"

struct FSkeletalMesh;
struct BoneWeights;

struct FFbxLoader
{
    static FSkeletalMesh* GetFbxObject(const FString& filename);

private:
    static FbxManager* GetFbxManager();
    static FbxIOSettings* GetFbxIOSettings();
    static FSkeletalMesh* LoadFBXObject(FbxScene* InFbxInfo);
    static void LoadFbxSkeleton(
        FSkeletalMesh* fbxObject,
        FbxNode* node,
        TMap<FString, int>& boneNameToIndex,
        int parentIndex
    );
    static void LoadSkinWeights(
        FbxNode* node,
        const TMap<FString, int>& boneNameToIndex,
        TMap<int, TArray<BoneWeights>>& OutBoneWeights
    );
    static void LoadFBXMesh(
        FSkeletalMesh* fbxObject,
        FbxNode* node,
        TMap<FString, int>& boneNameToIndex,
        TMap<int, TArray<BoneWeights>>& boneWeight
    );
    static void LoadFBXMaterials(
        FSkeletalMesh* fbxObject,
        FbxNode* node
    );
    static bool CreateTextureFromFile(const FWString& Filename, bool bIsSRGB);
    static void CalculateTangent(FFbxVertex& PivotVertex, const FFbxVertex& Vertex1, const FFbxVertex& Vertex2);
    inline static TMap<FString, FSkeletalMesh*> fbxMap;
    static FSkeletalMesh* ParseFBX(const FString& FBXFilePath);
};

