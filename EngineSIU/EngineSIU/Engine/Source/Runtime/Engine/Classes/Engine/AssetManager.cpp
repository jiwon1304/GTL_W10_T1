#include "AssetManager.h"
#include "Engine.h"

#include <filesystem>

#include "FFbxLoader.h"
#include "Engine/FObjLoader.h"

inline UAssetManager::UAssetManager() {
    FFbxLoader::Init();
    FFbxLoader::OnLoadFBXCompleted.BindLambda(
        [this](const FString& filename) {
            OnLoaded(filename);
        }
    );
    FFbxLoader::OnLoadFBXFailed.BindLambda(
        [this](const FString& filename) {
            OnFailed(filename);
        }
    );

    FObjManager::OnLoadOBJCompleted.BindLambda(
        [this](const FString& filename) {
            OnLoaded(filename);
        }
    );
    FObjManager::OnLoadOBJFailed.BindLambda(
        [this](const FString& filename) {
            OnFailed(filename);
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
        if (!FObjManager::GetStaticMesh(filePath))
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
    NewAssetInfo.State = FAssetInfo::LoadState::Completed;
    AssetRegistry->PathNameToAssetInfo.Add(NewAssetInfo.AssetName, NewAssetInfo);

    return true;
}

void UAssetManager::LoadEntireAssets()
{
    // Obj, FBX 파일 로드
    const std::string BasePathNameAssets = "Assets/";

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(BasePathNameAssets))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == ".obj")
        {
            FName NewAssetName = FName(Entry.path().filename().string());
            if (AssetRegistry->PathNameToAssetInfo.Contains(NewAssetName)) continue;
            FAssetInfo NewAssetInfo;
            NewAssetInfo.AssetName = NewAssetName;
            NewAssetInfo.PackagePath = FName(Entry.path().parent_path().string());
            NewAssetInfo.AssetType = EAssetType::StaticMesh; // obj 파일은 무조건 StaticMesh
            NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(Entry.path()));
            NewAssetInfo.State = FAssetInfo::LoadState::Loading; // obj는 비동기로 로드하므로 나중에 변경
            AssetRegistry->PathNameToAssetInfo.Add(NewAssetInfo.AssetName, NewAssetInfo);

            FString MeshName = NewAssetInfo.PackagePath.ToString() + "/" + NewAssetInfo.AssetName.ToString();
            FObjManager::CreateStaticMesh(MeshName);
            // ObjFileNames.push_back(UGTLStringLibrary::StringToWString(Entry.path().string()));
            // FObjManager::LoadObjStaticMeshAsset(UGTLStringLibrary::StringToWString(Entry.path().string()));
        }
        if (Entry.is_regular_file() && Entry.path().extension() == ".fbx")
        {
            FName NewAssetName = FName(Entry.path().filename().string());
            if (AssetRegistry->PathNameToAssetInfo.Contains(NewAssetName)) continue;
            FAssetInfo NewAssetInfo;
            NewAssetInfo.AssetName = NewAssetName;
            NewAssetInfo.PackagePath = FName(Entry.path().parent_path().string());
            NewAssetInfo.AssetType = EAssetType::SkeletalMesh;
            NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(Entry.path()));
            NewAssetInfo.State = FAssetInfo::LoadState::Loading; // fbx는 비동기로 로드하므로 나중에 변경
            AssetRegistry->PathNameToAssetInfo.Add(NewAssetInfo.AssetName, NewAssetInfo);

            FString MeshName = NewAssetInfo.PackagePath.ToString() + "/" + NewAssetInfo.AssetName.ToString();
            FFbxLoader::LoadFBX(MeshName);
        }
    }


    // Obj, FBX 파일 로드
    const std::string BasePathNameContents = "Contents/";
    
    for (const auto& Entry : std::filesystem::recursive_directory_iterator(BasePathNameContents))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == ".obj")
        {
            FName NewAssetName = FName(Entry.path().filename().string());
            if (AssetRegistry->PathNameToAssetInfo.Contains(NewAssetName)) continue;
            FAssetInfo NewAssetInfo;
            NewAssetInfo.AssetName = NewAssetName;
            NewAssetInfo.PackagePath = FName(Entry.path().parent_path().string());
            NewAssetInfo.AssetType = EAssetType::StaticMesh; // obj 파일은 무조건 StaticMesh
            NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(Entry.path()));
            NewAssetInfo.State = FAssetInfo::LoadState::Loading; // obj는 비동기로 로드하므로 나중에 변경
            AssetRegistry->PathNameToAssetInfo.Add(NewAssetInfo.AssetName, NewAssetInfo);
            
            FString MeshName = NewAssetInfo.PackagePath.ToString() + "/" + NewAssetInfo.AssetName.ToString();
            FObjManager::CreateStaticMesh(MeshName);
            // ObjFileNames.push_back(UGTLStringLibrary::StringToWString(Entry.path().string()));
            // FObjManager::LoadObjStaticMeshAsset(UGTLStringLibrary::StringToWString(Entry.path().string()));
        }
        if (Entry.is_regular_file() && Entry.path().extension() == ".fbx")
        {
            FName NewAssetName = FName(Entry.path().filename().string());
            if (AssetRegistry->PathNameToAssetInfo.Contains(NewAssetName)) continue;
            FAssetInfo NewAssetInfo;
            NewAssetInfo.AssetName = NewAssetName;
            NewAssetInfo.PackagePath = FName(Entry.path().parent_path().string());
            NewAssetInfo.AssetType = EAssetType::SkeletalMesh;
            NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(Entry.path()));
            NewAssetInfo.State = FAssetInfo::LoadState::Loading; // fbx는 비동기로 로드하므로 나중에 변경
            AssetRegistry->PathNameToAssetInfo.Add(NewAssetInfo.AssetName, NewAssetInfo);

            FString MeshName = NewAssetInfo.PackagePath.ToString() + "/" + NewAssetInfo.AssetName.ToString();
            FFbxLoader::LoadFBX(MeshName);
        }
    }
}

// 파일 로드의 호출이 UAssetManager 외부에서 발생하였을 때 등록하는 함수입니다.
void UAssetManager::RegisterAsset(std::wstring filePath, FAssetInfo::LoadState State)
{
    std::filesystem::path path(filePath);
    EAssetType assetType;
    if (path.extension() == ".fbx" || path.extension() == ".FBX")
    {
        assetType = EAssetType::SkeletalMesh;
    }
    else if (path.extension() == ".obj" || path.extension() == ".OBJ")
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
    NewAssetInfo.State = State;
    AssetRegistry->PathNameToAssetInfo.Add(NewAssetInfo.AssetName, NewAssetInfo);
}

void UAssetManager::OnLoaded(const FString& filename)
{
    FName AssetName = FName(filename);
    for (auto& asset : AssetRegistry->PathNameToAssetInfo)
    {
        if (asset.Value.AssetName == filename || asset.Value.GetFullPath() == filename)
        {
            asset.Value.State = FAssetInfo::LoadState::Completed;
            UE_LOG(ELogLevel::Display, "Asset loaded : %s", *filename);
            return;
        }
    }
    UE_LOG(ELogLevel::Warning, "Asset loaded but failed to register: %s", *filename);
    return;
}

void UAssetManager::OnFailed(const FString& filename)
{
    FName AssetName = FName(filename);
    for (auto& asset : AssetRegistry->PathNameToAssetInfo)
    {
        if (asset.Value.AssetName == filename || asset.Value.GetFullPath() == filename)
        {
            asset.Value.State = FAssetInfo::LoadState::Failed;
            UE_LOG(ELogLevel::Display, "Failed loading asset: %s", *filename);
            return;
        }
    }
    UE_LOG(ELogLevel::Warning, "Asset failed and failed to register: %s", *filename);
}
