#pragma once
#include <fbxsdk.h>

#include "FbxObject.h"
#include "Container/Array.h"
#include "Container/Map.h"
#include "Container/String.h"
#include "Asset/SkeletalMeshAsset.h"
#include <mutex>
#include "Delegates/DelegateCombination.h"


struct FFbxSkeletalMesh;
struct BoneWeights;
class USkeletalMesh;

DECLARE_DELEGATE_OneParam(FOnLoadFBXCompleted, const FString& /*filename*/);

struct FFbxLoader
{
public:
    static void Init();
    static void LoadFBX(const FString& filename);
    static USkeletalMesh* GetSkeletalMesh(const FString& filename);

    inline static FOnLoadFBXCompleted OnLoadFBXCompleted;
private:
    static USkeletalMesh* ParseSkeletalMesh(const FString& filename);
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
    inline static std::mutex MapMutex; // MeshEntry의 Map에 접근할 때 쓰는 뮤텍스
    inline static TMap<FString, MeshEntry> MeshMap;
};

struct FFbxManager
{
    static bool SaveFBXToBinary(const FWString& FilePath, int64_t LastModifiedTime, const FFbxSkeletalMesh& FBXObject);
    static bool LoadFBXFromBinary(const FWString& FilePath, int64_t LastModifiedTime, FFbxSkeletalMesh& OutFBXObject);
};
