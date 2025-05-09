#include "AssetManager.h"
#include "Engine.h"

#include <filesystem>

#include "FFbxLoader.h"
#include "Engine/FObjLoader.h"

inline UAssetManager::UAssetManager() {
    FFbxLoader::Init();
    FFbxLoader::OnLoadFBXCompleted.BindLambda(
        [this](const FString& filename) {
            UpdateLoadInfo(filename);
        }
    );
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

    // 디버그 시 로딩을 빠르게 하기 위해서 일부만 로드
#ifdef _DEBUG
    LoadEntireAssets();
#else
//    FFbxLoader::LoadFBX("Contents/dragon/Dragon 2.5_fbx.fbx");
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

void UAssetManager::LoadEntireAssets()
{
    const std::string BasePathName = "Contents/";

    // Obj, FBX 파일 로드
    
    for (const auto& Entry : std::filesystem::recursive_directory_iterator(BasePathName))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == ".obj")
        {
            continue;
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
            NewAssetInfo.IsLoaded = false; // fbx는 비동기로 로드하므로 나중에 변경
            AssetRegistry->PathNameToAssetInfo.Add(NewAssetInfo.AssetName, NewAssetInfo);

            FString MeshName = NewAssetInfo.PackagePath.ToString() + "/" + NewAssetInfo.AssetName.ToString();
            FFbxLoader::LoadFBX(MeshName);
        }
    }
}

// 파일 로드의 호출이 UAssetManager 외부에서 발생하였을 때 등록하는 함수입니다.
void UAssetManager::RegisterAsset(std::wstring filePath) const
{
    std::filesystem::path path(filePath);
    EAssetType assetType;
    if (path.extension() == ".fbx")
    {
        assetType = EAssetType::SkeletalMesh;
    }
    else if (path.extension() == ".obj")
    {
        assetType = EAssetType::StaticMesh;
    }
    else
    {
        return;
    }

    FAssetInfo NewAssetInfo;
    NewAssetInfo.AssetName = FName(path.filename().string());
    NewAssetInfo.PackagePath = FName(path.parent_path().string());
    NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(path));
    NewAssetInfo.AssetType = assetType;
    AssetRegistry->PathNameToAssetInfo.Add(NewAssetInfo.AssetName, NewAssetInfo);
}

void UAssetManager::UpdateLoadInfo(const FString& filename)
{
    FName AssetName = FName(filename);
    for (auto& asset : AssetRegistry->PathNameToAssetInfo)
    {
        if (asset.Value.GetFullPath() == filename)
        {
            asset.Value.IsLoaded = true;
            return;
        }
    }
}
