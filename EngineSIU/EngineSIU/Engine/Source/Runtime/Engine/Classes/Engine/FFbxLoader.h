#pragma once
#include <fbxsdk.h>

#include "Container/Array.h"
#include "Container/Map.h"
#include "Container/String.h"

struct FFbxObject;
struct BoneWeights;

struct FFbxLoader
{
    static FFbxObject* ParseFBX(const FString& FBXFilePath);
    static FFbxObject* GetFbxObject(const FString& filename) { return fbxMap[filename]; }
private:
    static FbxManager* GetFbxManager();
    static FbxIOSettings* GetFbxIOSettings();
    static FFbxObject* LoadFBXObject(FbxScene* InFbxInfo);
    static void LoadFbxSkeleton(
        FFbxObject* fbxObject,
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
        FFbxObject* fbxObject,
        FbxNode* node,
        TMap<FString, int>& boneNameToIndex,
        TMap<int, TArray<BoneWeights>>& boneWeight
    );
    static void LoadFBXMaterials(
        FFbxObject* fbxObject,
        FbxNode* node
    );
    inline static TMap<FString, FFbxObject*> fbxMap;
};

