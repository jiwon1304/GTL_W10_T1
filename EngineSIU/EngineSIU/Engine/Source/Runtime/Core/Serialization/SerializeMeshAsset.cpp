#include "SerializeMeshAsset.h"

#include <filesystem>
#include <fstream>
#include "MemoryArchive.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Components/Mesh/StaticMesh.h"
#include "Engine/AssetManager.h"
#include "UObject/ObjectFactory.h"

namespace fs = std::filesystem;


UStaticMeshTest* FSerializeMeshAsset::LoadStaticMeshFromBinary(const FString& FilePath)
{
    return nullptr;
}

USkeletalMesh* FSerializeMeshAsset::LoadSkeletalMeshFromBinary(const FString& FilePath)
{
    const fs::path Path = FilePath.ToWideString();

    TArray<uint8> LoadData;
    {
        std::ifstream InputStream{Path, std::ios::binary | std::ios::ate};
        if (!InputStream.is_open())
        {
            return nullptr;
        }
    
        const std::streamsize FileSize = InputStream.tellg();
        if (FileSize < 0)
        {
            // Error getting size
            InputStream.close();
            return nullptr;
        }
        if (FileSize == 0)
        {
            // Empty file is valid
            InputStream.close();
            return nullptr; // Buffer remains empty
        }
    
        InputStream.seekg(0, std::ios::beg);
    
        LoadData.SetNum(static_cast<int32>(FileSize));
        InputStream.read(reinterpret_cast<char*>(LoadData.GetData()), FileSize);
    
        if (InputStream.fail() || InputStream.gcount() != FileSize)
        {
            return nullptr;
        }
        InputStream.close();
    }

    FMemoryReader Reader{LoadData}; // TODO: FFileArchive 만들기

    // TODO: 여러번 불러오면 오브젝트가 중복 생성됨, 나중에 리임포트 기능 만들면 수정 필요
    UAssetManager& AssetManager = UAssetManager::Get();
    USkeletalMesh* NewSkeletalMesh = FObjectFactory::ConstructObject<USkeletalMesh>(&AssetManager);
    SerializeVersion(Reader);
    SerializeMeshAsset(Reader, NewSkeletalMesh);

    FAssetInfo& Info = AssetManager.GetAssetRegistry().FindOrAdd(Path.filename().c_str());
    Info.AssetName = FName(Path.filename().wstring());
    Info.AssetType = EAssetType::SkeletalMesh;
    Info.PackagePath = FName(Path.parent_path().generic_wstring());
    Info.Size = static_cast<uint32>(std::filesystem::file_size(Path));

    FEngineLoop::ResourceManager.AddAssignSkeletalMeshMap(FName(Path.generic_wstring()), NewSkeletalMesh);
    NewSkeletalMesh->AssetName = Info.AssetName;
    return NewSkeletalMesh;
}

bool FSerializeMeshAsset::SaveStaticMeshToBinary(const FString& FilePath, UStaticMeshTest* StaticMesh)
{
    return false;
}

bool FSerializeMeshAsset::SaveSkeletalMeshToBinary(const FString& FilePath, USkeletalMesh* SkeletalMesh)
{
    const fs::path Path = FilePath.ToWideString();

    TArray<uint8> SaveData;
    FMemoryWriter Writer{SaveData}; // TODO: FFileArchive 만들기

    SerializeVersion(Writer);
    SerializeMeshAsset(Writer, SkeletalMesh);

    std::ofstream OutputStream{Path, std::ios::binary | std::ios::trunc};
    if (!OutputStream.is_open())
    {
        return false;
    }

    if (SaveData.Num() > 0)
    {
        OutputStream.write(reinterpret_cast<const char*>(SaveData.GetData()), SaveData.Num());

        if (OutputStream.fail())
        {
            return false;
        }
    }

    OutputStream.close();
    return true;
}

FArchive& FSerializeMeshAsset::SerializeVersion(FArchive& Ar)
{
    if (Ar.IsSaving())
    {
        return Ar << Version;
    }

    uint32 FileVersion;
    Ar << FileVersion;

    if (FileVersion != Version)
    {
        UE_LOG_FMT(ELogLevel::Error, "MeshAsset version mismatch: {} != {}", FileVersion, Version);
    }

    return Ar;
}

FArchive& FSerializeMeshAsset::SerializeMeshAssetBase(FArchive& Ar, UMeshAsset* Mesh)
{
    return Ar << Mesh->AssetName
              << Mesh->Materials;
}

FArchive& FSerializeMeshAsset::SerializeMeshAsset(FArchive& Ar, UStaticMeshTest* StaticMesh)
{
    SerializeMeshAssetBase(Ar, StaticMesh);

    return Ar << StaticMesh->Vertices
              << StaticMesh->Indices
              << StaticMesh->Subsets;
}

FArchive& FSerializeMeshAsset::SerializeMeshAsset(FArchive& Ar, USkeletalMesh* SkeletalMesh)
{
    SerializeMeshAssetBase(Ar, SkeletalMesh);

    return Ar << SkeletalMesh->Vertices
              << SkeletalMesh->Indices
              << SkeletalMesh->Subsets
              << SkeletalMesh->SkeletonData
              << SkeletalMesh->AnimationClips
              << SkeletalMesh->bHasAnimationData;
}
