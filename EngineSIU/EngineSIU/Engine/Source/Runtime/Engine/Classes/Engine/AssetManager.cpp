#include "AssetManager.h"
#include "Engine.h"

#include <filesystem>

#include "FbxManager.h"
#include "Engine/FObjLoader.h"

inline UAssetManager::UAssetManager() {
    FFbxManager::Init();
    FFbxManager::OnLoadFBXCompleted.BindLambda(
        [this](const FString& filename) {
            OnLoaded(filename);
        }
    );
    FFbxManager::OnLoadFBXFailed.BindLambda(
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

bool UAssetManager::AddAsset(FString filePath)
{
    std::wstring wFilePath = filePath.ToWideString();
    std::filesystem::path path(wFilePath);
    EAssetType assetType;
    if (path.extension() == ".fbx")
    {
        assetType = EAssetType::SkeletalMesh;
        FFbxManager::Load(filePath);
        return true;

        //if (!FFbxManager::GetSkeletalMesh(filePath))
            //return false;
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

    
    //FAssetInfo NewAssetInfo;
    //NewAssetInfo.AssetName = FName(path.filename().string());
    //NewAssetInfo.PackagePath = FName(path.parent_path().string());
    //NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(path));
    //NewAssetInfo.AssetType = assetType;
    //NewAssetInfo.State = FAssetInfo::LoadState::Completed;
    //AddAssetInternal(NewAssetInfo.AssetName, NewAssetInfo);

    //return true;
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
            if (ContainsInternal(NewAssetName)) continue;
            FAssetInfo NewAssetInfo;
            NewAssetInfo.AssetName = NewAssetName;
            NewAssetInfo.PackagePath = FName(Entry.path().parent_path().string());
            NewAssetInfo.AssetType = EAssetType::StaticMesh; // obj 파일은 무조건 StaticMesh
            NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(Entry.path()));
            NewAssetInfo.State = FAssetInfo::LoadState::Loading; // obj는 비동기로 로드하므로 나중에 변경
            AddAssetInternal(NewAssetInfo.AssetName, NewAssetInfo);

            FString MeshName = NewAssetInfo.PackagePath.ToString() + "/" + NewAssetInfo.AssetName.ToString();
            FObjManager::CreateStaticMesh(MeshName);
            // ObjFileNames.push_back(UGTLStringLibrary::StringToWString(Entry.path().string()));
            // FObjManager::LoadObjStaticMeshAsset(UGTLStringLibrary::StringToWString(Entry.path().string()));
        }
        if (Entry.is_regular_file() && Entry.path().extension() == ".fbx")
        {
            FName NewAssetName = FName(Entry.path().filename().string());
            if (ContainsInternal(NewAssetName)) continue;
            FAssetInfo NewAssetInfo;
            NewAssetInfo.AssetName = NewAssetName;
            NewAssetInfo.PackagePath = FName(Entry.path().parent_path().string());
            NewAssetInfo.AssetType = EAssetType::SkeletalMesh;
            NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(Entry.path()));
            NewAssetInfo.State = FAssetInfo::LoadState::Loading; // fbx는 비동기로 로드하므로 나중에 변경
            AddAssetInternal(NewAssetInfo.AssetName, NewAssetInfo);

            FString MeshName = NewAssetInfo.PackagePath.ToString() + "/" + NewAssetInfo.AssetName.ToString();
            FFbxManager::Load(MeshName);
        }
    }


    // Obj, FBX 파일 로드
    const std::string BasePathNameContents = "Contents/";
    
    for (const auto& Entry : std::filesystem::recursive_directory_iterator(BasePathNameContents))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == ".obj")
        {
            FName NewAssetName = FName(Entry.path().filename().string());
            if (ContainsInternal(NewAssetName)) continue;
            FAssetInfo NewAssetInfo;
            NewAssetInfo.AssetName = NewAssetName;
            NewAssetInfo.PackagePath = FName(Entry.path().parent_path().string());
            NewAssetInfo.AssetType = EAssetType::StaticMesh; // obj 파일은 무조건 StaticMesh
            NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(Entry.path()));
            NewAssetInfo.State = FAssetInfo::LoadState::Loading; // obj는 비동기로 로드하므로 나중에 변경
            AddAssetInternal(NewAssetInfo.AssetName, NewAssetInfo);

            FString MeshName = NewAssetInfo.PackagePath.ToString() + "/" + NewAssetInfo.AssetName.ToString();
            FObjManager::CreateStaticMesh(MeshName);
            // ObjFileNames.push_back(UGTLStringLibrary::StringToWString(Entry.path().string()));
            // FObjManager::LoadObjStaticMeshAsset(UGTLStringLibrary::StringToWString(Entry.path().string()));
        }
        if (Entry.is_regular_file() && Entry.path().extension() == ".fbx")
        {
            FName NewAssetName = FName(Entry.path().filename().string());
            if (ContainsInternal(NewAssetName)) continue;
            FAssetInfo NewAssetInfo;
            NewAssetInfo.AssetName = NewAssetName;
            NewAssetInfo.PackagePath = FName(Entry.path().parent_path().string());
            NewAssetInfo.AssetType = EAssetType::SkeletalMesh;
            NewAssetInfo.Size = static_cast<uint32>(std::filesystem::file_size(Entry.path()));
            NewAssetInfo.State = FAssetInfo::LoadState::Loading; // fbx는 비동기로 로드하므로 나중에 변경
            AddAssetInternal(NewAssetInfo.AssetName, NewAssetInfo);

            FString MeshName = NewAssetInfo.PackagePath.ToString() + "/" + NewAssetInfo.AssetName.ToString();
            FFbxManager::Load(MeshName);
        }
    }
}

// 파일 로드의 호출이 UAssetManager 외부에서 발생하였을 때 등록하는 함수입니다.
void UAssetManager::RegisterAsset(FString filePath, FAssetInfo::LoadState State)
{
    std::wstring wFilePath = filePath.ToWideString();
    std::filesystem::path path(wFilePath);
    EAssetType assetType;
    if (!std::filesystem::exists(wFilePath))
    {
        return;
    }

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
    AddAssetInternal(NewAssetInfo.AssetName, NewAssetInfo);
}

void UAssetManager::RemoveAsset(FString filePath)
{
    FSpinLockGuard Lock(RegistryLock);
    AssetRegistry->PathNameToAssetInfo.Remove(filePath);
}

void UAssetManager::OnLoaded(const FString& filename)
{
    FName AssetName = FName(filename);
    FSpinLockGuard Lock(RegistryLock);
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
    FSpinLockGuard Lock(RegistryLock);
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

// thread-safe
// 이미 있으면 false를 리턴
bool UAssetManager::AddAssetInternal(const FName& AssetName, const FAssetInfo& AssetInfo)
{
    FSpinLockGuard Lock(RegistryLock);
    if (AssetRegistry->PathNameToAssetInfo.Contains(AssetName))
    {
        return false;
    }
    else
    {
        AssetRegistry->PathNameToAssetInfo.Add(AssetName, AssetInfo);
        return true;
    }
}

// 없으면 false를 리턴
bool UAssetManager::SetAssetInternal(const FName& AssetName, const FAssetInfo& AssetInfo)
{
    FSpinLockGuard Lock(RegistryLock);
    if (AssetRegistry->PathNameToAssetInfo.Contains(AssetName))
    {
        AssetRegistry->PathNameToAssetInfo[AssetName] = AssetInfo;
        return true;
    }
    else
    {
        return false;
    }
}

bool UAssetManager::ContainsInternal(const FName& AssetName)
{
    FSpinLockGuard Lock(RegistryLock);
    return AssetRegistry->PathNameToAssetInfo.Contains(AssetName);
}
