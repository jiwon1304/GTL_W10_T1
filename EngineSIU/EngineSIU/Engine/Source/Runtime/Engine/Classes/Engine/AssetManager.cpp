#include "AssetManager.h"
#include "Engine.h"

#include <filesystem>

#include "FFbxLoader.h"
#include "Engine/FObjLoader.h"

inline UAssetManager::UAssetManager() {
    FFbxLoader::Init();
}

bool UAssetManager::IsInitialized()
{
    return GEngine && GEngine->AssetManager;
}

UAssetManager& UAssetManager::Get()
{
    if (UAssetManager* Singleton = GEngine->AssetManager)
    {
        return *Singleton;
    }
    else
    {
        UE_LOG(ELogLevel::Error, "Cannot use AssetManager if no AssetManagerClassName is defined!");
        assert(0);
        return *new UAssetManager; // never calls this
    }
}

UAssetManager* UAssetManager::GetIfInitialized()
{
    return GEngine ? GEngine->AssetManager : nullptr;
}

void UAssetManager::InitAssetManager()
{
    AssetRegistry = std::make_unique<FAssetRegistry>();

#ifndef DEBUG
    LoadObjFiles();
#endif // DEBUG
}

const TMap<FName, FAssetInfo>& UAssetManager::GetAssetRegistry()
{
    return AssetRegistry->PathNameToAssetInfo;
}

bool UAssetManager::AddAsset(std::wstring filePath) const
{
    std::filesystem::path path(filePath);
    EAssetType assetType;
    if (path.extension() == ".fbx")
    {
        assetType = EAssetType::SkeletalMesh;
        if (!FFbxLoader::GetSkeletalMesh(filePath))
            return false;
    }
    else if (path.extension() == ".obj")
    {
        assetType = EAssetType::StaticMesh;
        if (!FObjManager::CreateStaticMesh(filePath))
            return false;
    } else
    {
        return false;
    }
    
    FAssetInfo NewAssetInfo;
    NewAssetInfo.AssetName = FName(path.filename().string());
    NewAssetInfo.PackagePath = FName(path.parent_path().string());
    NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(path));
    NewAssetInfo.AssetType = assetType;
    AssetRegistry->PathNameToAssetInfo.Add(NewAssetInfo.AssetName, NewAssetInfo);

    return true;
}

void UAssetManager::LoadObjFiles()
{
    const std::string BasePathName = "Contents/";

    // Obj, FBX 파일 로드
    
    for (const auto& Entry : std::filesystem::recursive_directory_iterator(BasePathName))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == ".obj")
        {
            FAssetInfo NewAssetInfo;
            NewAssetInfo.AssetName = FName(Entry.path().filename().string());
            NewAssetInfo.PackagePath = FName(Entry.path().parent_path().string());
            NewAssetInfo.AssetType = EAssetType::StaticMesh; // obj 파일은 무조건 StaticMesh
            NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(Entry.path()));
            
            AssetRegistry->PathNameToAssetInfo.Add(NewAssetInfo.AssetName, NewAssetInfo);
            
            FString MeshName = NewAssetInfo.PackagePath.ToString() + "/" + NewAssetInfo.AssetName.ToString();
            FObjManager::CreateStaticMesh(MeshName);
            // ObjFileNames.push_back(UGTLStringLibrary::StringToWString(Entry.path().string()));
            // FObjManager::LoadObjStaticMeshAsset(UGTLStringLibrary::StringToWString(Entry.path().string()));
        }
        if (Entry.is_regular_file() && Entry.path().extension() == ".fbx")
        {
            FAssetInfo NewAssetInfo;
            NewAssetInfo.AssetName = FName(Entry.path().filename().string());
            NewAssetInfo.PackagePath = FName(Entry.path().parent_path().string());
            NewAssetInfo.AssetType = EAssetType::SkeletalMesh;
            NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(Entry.path()));
            AssetRegistry->PathNameToAssetInfo.Add(NewAssetInfo.AssetName, NewAssetInfo);

            FString MeshName = NewAssetInfo.PackagePath.ToString() + "/" + NewAssetInfo.AssetName.ToString();
            FFbxLoader::LoadFBX(MeshName);
        }
    }
}
