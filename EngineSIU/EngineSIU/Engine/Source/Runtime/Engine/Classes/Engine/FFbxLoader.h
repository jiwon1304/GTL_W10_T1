#pragma once
#include <fbxsdk.h>

#include "FbxObject.h"
#include "Container/Array.h"
#include "Container/Map.h"
#include "Container/String.h"
#include "Asset/SkeletalMeshAsset.h"
#include <mutex>

#include "Animation/AnimSequence.h"
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
    static FFbxSkeletalMesh* ParseFBX(const FString& FBXFilePath, USkeletalMesh* Mesh);
    static FbxIOSettings* GetFbxIOSettings();
    static FbxCluster* FindClusterForBone(FbxNode* boneNode);
    static FFbxSkeletalMesh* LoadFBXObject(FbxScene* InFbxScene, USkeletalMesh* Mesh);
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
    static void ParseFBXAnimationOnly(const FString& filename, USkeletalMesh* skeletalMesh);
    static void LoadFBXMesh(
        FFbxSkeletalMesh* fbxObject,
        FbxNode* node,
        TMap<FString, int>& boneNameToIndex,
        TMap<int, TArray<BoneWeights>>& boneWeight
    );
    static void LoadAnimationInfo(
        FbxScene* Scene, USkeletalMesh* SkeletalMesh, TArray<UAnimSequence*>& OutSequences
    );
    static void LoadAnimationData(
        FbxScene* Scene, FbxNode* RootNode, USkeletalMesh* SkeletalMesh, UAnimSequence* Sequence
    );

    static void DumpAnimationDebug(const FString& FBXFilePath, const USkeletalMesh* SkeletalMesh, const TArray<UAnimSequence*>& AnimSequences);

    static bool CreateTextureFromFile(const FWString& Filename, bool bIsSRGB);
    static void LoadFBXMaterials(
        FFbxSkeletalMesh* fbxObject,
        FbxNode* node
    );
    static void CalculateTangent(FFbxVertex& PivotVertex, const FFbxVertex& Vertex1, const FFbxVertex& Vertex2);
    static FbxNode* FindBoneNode(FbxNode* Root, const FString& BoneName);
    static FTransform FTransformFromFbxMatrix(const FbxAMatrix& Matrix);
    //inline static TArray<FSkeletalMeshRenderData> RenderDatas; // 일단 Loader에서 가지고 있게 함

    // 비동기용 로드 상태
    inline static std::mutex SDKMutex;
    inline static FbxManager* Manager;
public:
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
    struct FAnimEntry
    {
        LoadState State;
        UAnimSequence* Sequence;
    };
    static const FbxAxisSystem UnrealTargetAxisSystem;
    inline static const FQuat FinalBoneCorrectionQuat = FQuat(FVector(0, 0, 1), FMath::DegreesToRadians(-90.0f));

private:
    inline static std::mutex MapMutex; // MeshEntry의 Map에 접근할 때 쓰는 뮤텍스

    inline static TMap<FString, MeshEntry> MeshMap;
public:
    inline static std::mutex AnimMapMutex; // AnimEntry의 Map에 접근할 때 쓰는 뮤텍스
    inline static TMap<FString, FAnimEntry> AnimMap;

    static UAnimSequence* GetAnimSequenceByName(const FString& SequenceName);
};

struct FFbxManager
{
    static bool SaveFBXToBinary(const FWString& FilePath, int64_t LastModifiedTime, const FFbxSkeletalMesh& FBXObject);
    static bool LoadFBXFromBinary(const FWString& FilePath, int64_t LastModifiedTime, FFbxSkeletalMesh& OutFBXObject);
};
