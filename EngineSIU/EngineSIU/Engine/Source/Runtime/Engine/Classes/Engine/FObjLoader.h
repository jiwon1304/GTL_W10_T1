#pragma once

#include "EngineLoop.h"
#include "Container/Map.h"
#include "HAL/PlatformType.h"
#include "Serialization/Serializer.h"
#include <mutex>
#include "Core/Misc/Spinlock.h"

class UStaticMesh;
struct FObjManager;

struct FStaticMeshVertex;
struct FStaticMeshRenderData;

DECLARE_DELEGATE_OneParam(FOnLoadOBJCompleted, const FString& /*filename*/);
DECLARE_DELEGATE_OneParam(FOnLoadOBJFailed, const FString& /*filename*/);

struct FObjLoader
{
    // Obj Parsing (*.obj to FObjInfo)
    static bool ParseOBJ(const FString& ObjFilePath, FObjInfo& OutObjInfo);

    // Material Parsing (*.obj to MaterialInfo)
    static bool ParseMaterial(FObjInfo& OutObjInfo, FStaticMeshRenderData& OutFStaticMesh);

    // Convert the Raw data to Cooked data (FStaticMeshRenderData)
    static bool ConvertToRenderData(const FObjInfo& RawData, FStaticMeshRenderData& OutStaticMesh);

    static bool CreateTextureFromFile(const FWString& Filename, bool bIsSRGB = true);

    static void ComputeBoundingBox(const TArray<FStaticMeshVertex>& InVertices, FVector& OutMinVector, FVector& OutMaxVector);

private:
    static void CalculateTangent(FStaticMeshVertex& PivotVertex, const FStaticMeshVertex& Vertex1, const FStaticMeshVertex& Vertex2);
};

struct FObjManager
{
public:
    inline static FOnLoadOBJCompleted OnLoadOBJCompleted;
    inline static FOnLoadOBJCompleted OnLoadOBJFailed;

    static UMaterial* GetMaterial(FString name);

    static int GetMaterialNum() { return MaterialMap.Num(); }

    static void CreateStaticMesh(const FString& filePath);

    static UStaticMesh* GetStaticMesh(const FString& filename);

    static FStaticMeshRenderData LoadObjStaticMeshAsset(const FString& PathFileName);

    static UMaterial* CreateMaterial(const FMaterialInfo& materialInfo);

    static TMap<FString, UMaterial*>& GetMaterials() { return MaterialMap; }
private:
    static void CombineMaterialIndex(FStaticMeshRenderData& OutFStaticMesh);

    static bool SaveStaticMeshToBinary(const FWString& FilePath, const FStaticMeshRenderData& StaticMesh);

    static bool LoadStaticMeshFromBinary(const FWString& FilePath, FStaticMeshRenderData& OutStaticMesh);



private:
    //inline static TMap<FString, FStaticMeshRenderData*> ObjStaticMeshMap;
    //inline static TMap<FWString, UStaticMesh*> StaticMeshMap;
    inline static TMap<FString, UMaterial*> MaterialMap;

    enum class LoadState
    {
        Loading,
        Completed,
        Failed,
        None
    };
    struct MeshEntry {
        LoadState State;
        UStaticMesh* Mesh;
    };
    inline static FSpinLock MapLock;

    inline static TMap<FString, MeshEntry> MeshMap;

    static LoadState GetState(const FString& filename)
    {
        FSpinLockGuard lock(MapLock);
        if (MeshMap.Contains(filename))
        {
            return MeshMap[filename].State;
        }
        return LoadState::None;
    }
    static void SetState(const FString& filename, LoadState state)
    {
        FSpinLockGuard lock(MapLock);
        MeshMap[filename].State = state;
    }
    // 이미 등록되어있으면 false를 반환
    static bool SetCompleted(const FString& filename, UStaticMesh* Mesh)
    {
        FSpinLockGuard lock(MapLock);
        if (!MeshMap.Contains(filename))
        {
            return false;
        }
        MeshMap[filename] = { LoadState::Completed, Mesh };
    }
    static bool SetFailed(const FString& filename)
    {
        FSpinLockGuard lock(MapLock);
        if (!MeshMap.Contains(filename))
        {
            return false;
        }
        MeshMap[filename] = { LoadState::Failed, nullptr };
    }
    static bool IsRegistered(const FString& filename)
    {
        FSpinLockGuard lock(MapLock);
        return MeshMap.Contains(filename);
    }
};
