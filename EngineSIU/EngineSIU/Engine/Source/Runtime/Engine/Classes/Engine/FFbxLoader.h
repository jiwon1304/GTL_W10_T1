#pragma once
#include <fbxsdk.h>

#include "FbxObject.h"
#include "Container/Array.h"
#include "Container/Map.h"
#include "Container/String.h"
#include "Asset/SkeletalMeshAsset.h"

struct FFbxSkeletalMesh;
struct BoneWeights;
class USkeletalMesh;

struct FFbxLoader // TODO: 나중에 이름 바꿔야 할듯
{
    static USkeletalMesh* GetFbxObject(const FString& FilePath);
    static USkeletalMesh* LoadBinaryObject(const FString& FilePath);
    static bool SaveBinaryObject(const FString& FilePath, USkeletalMesh* SkeletalMesh);

private:
    static FFbxSkeletalMesh* GetFbxObjectInternal(const FString& filename);
    static FbxManager* GetFbxManager();
    static FbxIOSettings* GetFbxIOSettings();
    static FbxCluster* FindClusterForBone(FbxNode* boneNode);
    static FFbxSkeletalMesh* LoadFBXObject(FbxScene* InFbxInfo);
    static void LoadFbxSkeleton(
        FFbxSkeletalMesh* fbxObject,
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
        FFbxSkeletalMesh* fbxObject,
        FbxNode* node,
        TMap<FString, int>& boneNameToIndex,
        TMap<int, TArray<BoneWeights>>& boneWeight
    );
    static void LoadFBXMaterials(
        FFbxSkeletalMesh* fbxObject,
        FbxNode* node
    );
    static bool CreateTextureFromFile(const FWString& Filename, bool bIsSRGB);
    static void CalculateTangent(FFbxVertex& PivotVertex, const FFbxVertex& Vertex1, const FFbxVertex& Vertex2);
    inline static TMap<FString, FFbxSkeletalMesh*> fbxMap;
    inline static TMap<FString, USkeletalMesh*> SkeletalMeshMap;
    //inline static TArray<FSkeletalMeshRenderData> RenderDatas; // 일단 Loader에서 가지고 있게 함
    static FFbxSkeletalMesh* ParseFBX(const FString& FBXFilePath);
};
