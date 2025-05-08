#pragma once
#include <fbxsdk.h>

#include "FbxObject.h"
#include "Container/Array.h"
#include "Container/Map.h"
#include "Container/String.h"
#include "Asset/SkeletalMeshAsset.h"
#include <mutex>

struct FFbxSkeletalMesh;
struct BoneWeights;
class USkeletalMesh;

struct FFbxLoader
{
public:
    static void Init();
    static void LoadFBX(const FString& filename);
    static USkeletalMesh* GetSkeletalMesh(const FString& filename);
private:
    static USkeletalMesh* GetFbxObject(const FString& filename);
    static FFbxSkeletalMesh* ParseFBX(const FString& FBXFilePath);
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
    inline static TMap<FString, USkeletalMesh*> SkeletalMeshMap;
    //inline static TArray<FSkeletalMeshRenderData> RenderDatas; // 일단 Loader에서 가지고 있게 함

    // 비동기용 로드 상태
    inline static std::mutex SDKMutex;
    inline static FbxManager* Manager;
    enum class LoadState
    {
        Loading,
        Completed,
        Failed
    };
    struct MeshEntry {
        LoadState State;
        USkeletalMesh* Mesh;
    };
    inline static std::mutex MapMutex;
    inline static TMap<FString, MeshEntry> MeshMap;
};
