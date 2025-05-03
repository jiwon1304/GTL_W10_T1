#pragma once

#include <fbxsdk.h>
#include "EngineLoop.h"
#include "Container/Map.h"
#include "HAL/PlatformType.h"
#include "Serialization/Serializer.h"

class USkeletalMesh;
struct FObjManager;

struct FSkeletalMeshRenderData;


class FFbxLoader
{
public:
    // Parse FBX Mesh (*.fbx input format)
    static bool ParseFBX(const FWString& FilePath, FSkeletalMeshRenderData& OutRenderData);

private:
    static void LoadTexture(FbxScene* Scene, const FWString FilePath);
    //static void LoadSkeletal
    static void LoadSceneRecursive(FbxScene* Scene, FSkeletalMeshRenderData& OutRenderData);
    static void LoadNodeRecursive(FbxNode* Node, FSkeletalMeshRenderData& OutRenderData);

    static void ParseMesh(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData, uint32 VertexBase);
    static void ParseMeshByMaterial(FbxNode* Node, FSkeletalMeshRenderData& OutRenderData);
    static void ParseMaterial(FbxSurfaceMaterial* Material, FMaterialInfo& OutMaterialInfo);
    static void ParseSkeleton(FbxNode* Node, FSkeletalMeshRenderData& OutRenderData, uint32 VertexBase);
    static void ParseSkinningData(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData, uint32 VertexBase);

    static FbxManager* SDKManager;
    static FbxIOSettings* IoSettings;
};

class FFbxManager
{
public:
    static FSkeletalMeshRenderData* LoadFbxSkeletalMeshAsset(const FWString& PathFileName);
    static UMaterial* CreateMaterial(const FMaterialInfo& MaterialInfo);

    static bool SaveSkeletalMeshToBinary(const FWString& FilePath, const FSkeletalMeshRenderData& SkeletalMesh);
    static bool LoadSkeletalMeshFromBinary(const FWString& FilePath, FSkeletalMeshRenderData& OutSkeletalMesh);
    static void ParseFbxNode(FbxNode* Node, FSkeletalMeshRenderData& OutRenderData);
    static void ParseMesh(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData);
    static void ParseMaterial(FbxSurfaceMaterial* Material, FMaterialInfo& OutMaterialInfo);

    static USkeletalMesh* CreateSkeletalMesh(const FWString& PathFile);
    static USkeletalMesh* GetSkeletalMesh(const FWString& ObjectName);

private:
    inline static TMap<FString, FSkeletalMeshRenderData> FbxSkeletalMeshMap;
    inline static TMap<FWString, USkeletalMesh*> SkeletalMeshMap;
    inline static TMap<FString, UMaterial*> MaterialMap;
};
