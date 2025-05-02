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
    static bool ParseFBX(const FString& FilePath, FSkeletalMeshRenderData& OutRenderData);

private:
    static void ParseMesh(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData);
    static void ParseSkeleton(FbxNode* RootNode, FSkeletalMeshRenderData& OutRenderData);
    static void ParseSkinningData(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData);

    static FbxManager* FbxManager;
    static FbxIOSettings* IoSettings;
};

class FFbxManager
{
public:
    static FSkeletalMeshRenderData* LoadFbxSkeletalMeshAsset(const FString& PathFileName);
    static UMaterial* CreateMaterial(const FMaterialInfo& MaterialInfo);

    static bool SaveSkeletalMeshToBinary(const FWString& FilePath, const FSkeletalMeshRenderData& SkeletalMesh);
    static bool LoadSkeletalMeshFromBinary(const FWString& FilePath, FSkeletalMeshRenderData& OutSkeletalMesh);
    static void ParseFbxNode(FbxNode* Node, FSkeletalMeshRenderData& OutRenderData);
    static void ParseMesh(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData);
    static void ParseMaterial(FbxSurfaceMaterial* Material, FMaterialInfo& OutMaterialInfo);

    static USkeletalMesh* CreateSkeletalMesh(const FString& PathFile);
    static USkeletalMesh* GetSkeletalMesh(const FWString& ObjectName);

private:
    inline static TMap<FString, FSkeletalMeshRenderData*> FbxSkeletalMeshMap;
    inline static TMap<FWString, USkeletalMesh*> SkeletalMeshMap;
    inline static TMap<FString, UMaterial*> MaterialMap;
};
