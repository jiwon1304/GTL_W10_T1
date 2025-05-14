#include "FbxManager.h"
#include "FFbxLoader.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include "Serializer.h"
#include "Engine/AssetManager.h"
#include "Components/Material/Material.h"
#include "UObject/ObjectFactory.h"
#include "Runtime/Engine/Animation/AnimNotifies/AnimNotifyState.h"

void FFbxManager::Init()
{
    FFbxLoader::Init();
    LoadThread = std::thread(&FFbxManager::LoadFunc);
    SaveThread = std::thread(&FFbxManager::SaveFunc);
    ConvertThread = std::thread(&FFbxManager::ConvertFunc);

    LoadThread.detach();
    SaveThread.detach();
    ConvertThread.detach();
}

void FFbxManager::Shutdown()
{
    bStopThread = true;
    ConvertCondition.notify_all();
    SaveCondition.notify_all();
    LoadCondition.notify_all();
    {
        std::unique_lock<std::mutex> LockL(LoadTerminatedMutex);
        LoadTerminatedCondition.wait(LockL, [&] { return bLoadThreadTerminated; });
    }
    {
        std::unique_lock<std::mutex> LockC(ConvertTerminatedMutex);
        ConvertTerminatedCondition.wait(LockC, [&] { return bConvertThreadTerminated; });
    }
    {
        std::unique_lock<std::mutex> LockS(SaveTerminatedMutex);
        SaveTerminatedCondition.wait(LockS, [&] { return bSaveThreadTerminated; });
    }

    // 1. FileName별로 AnimName-Notifies 맵 만들기
    TMap<FString, TMap<FString, TArray<FAnimNotifyEvent>>> FileNameToAnimNotifiesMap;
    for (const auto& pair : AnimMap)
    {
        if (pair.Value.State != LoadState::Completed)
            continue;
        const FString& FileName = pair.Value.FileName;
        UAnimSequence* Sequence = pair.Value.Sequence;
        if (!Sequence) continue;
        FileNameToAnimNotifiesMap
            .FindOrAdd(FileName)
            .Add(Sequence->GetSeqName(), Sequence->Notifies);
    }

    // 2. FileName별로 .notifies 파일 저장
    for (const auto& pair : FileNameToAnimNotifiesMap)
    {
        const FString& FileName = pair.Key;
        const TMap<FString, TArray<FAnimNotifyEvent>>& AnimNotifiesMap = pair.Value;
        FString NotifyFileName = FileName + TEXT(".notifies");
        SaveNotifiesToBinary(StringToWString(*NotifyFileName), AnimNotifiesMap);
    }

    // 3. FbxMeshMap 정리
    for (auto& pair : FbxMeshMap)
    {
        delete pair.Value;
    }
}

void FFbxManager::Load(const FString& filename, bool bPrioritized)
{
    if (filename.IsEmpty())
        return;
    {
        FSpinLockGuard Lock(MeshMapLock);
        if (MeshMap.Contains(filename))
        {
            if (MeshMap[filename].State != LoadState::Failed)
            {
                UE_LOG(ELogLevel::Display, TEXT("Already Loaded: %s"), *filename);
                return;
            }
            else
            {
                MeshMap.Remove(filename);
                UAssetManager::Get().RemoveAsset(filename);
                UE_LOG(ELogLevel::Warning, TEXT("Already Loaded but failed: %s"), *filename);
                UE_LOG(ELogLevel::Warning, TEXT("Trying reload: %s"), *filename);
            }
        }
        MeshMap.Add(filename, { LoadState::Queued, bPrioritized, nullptr });
    }

    // 로드 큐에 추가
    if (bPrioritized)
    {
        PriorityLoadQueue.Enqueue(filename);
    }
    else
    {
        LoadQueue.Enqueue(filename);
    }
    LoadCondition.notify_one();
}

USkeletalMesh* FFbxManager::GetSkeletalMesh(const FString& filename)
{
    {
        FSpinLockGuard Lock(MeshMapLock);
        if (MeshMap.Contains(filename))
        {
            if (MeshMap[filename].State == LoadState::Completed)
            {
                return MeshMap[filename].Mesh;
            }
        }
    }
    UE_LOG(ELogLevel::Warning, TEXT("GetSkeletalMesh: %s not found or not loaded yet."), *filename);
    return nullptr;
}

UAnimSequence* FFbxManager::GetAnimSequenceByName(const FString& SeqName)
{
    {
        FSpinLockGuard Lock(AnimMapLock);
        if (AnimMap.Contains(SeqName))
        {
            if (AnimMap[SeqName].State == LoadState::Completed)
            {
                return AnimMap[SeqName].Sequence;
            }
        }
    }
    UE_LOG(ELogLevel::Warning, TEXT("GetAnimSequence: %s not found or not loaded yet."), *SeqName);
    return nullptr;
}

// !!! 스핀락 적용 필수
// 예시
//{ /* scope 지정 */
//    FSpinLockGuard Lock(FFbxManager::MeshMapLock);
//    FFbxManager::GetSkeletalMeshes();
//    /* Do something*/
//}
const TMap<FString, FFbxManager::MeshEntry>& FFbxManager::GetSkeletalMeshes()
{
    return MeshMap;
}

// !!! 스핀락 적용 필수
const TMap<FString, FFbxManager::AnimEntry>& FFbxManager::GetAnimSequences()
{
    return AnimMap;
}

bool FFbxManager::IsPriorityQueueDone()
{
    if (!PriorityLoadQueue.IsEmpty() || !PriorityConvertQueue.IsEmpty())
        return false;

    FSpinLockGuard Lock(MeshMapLock);
    for (const auto& Pair : MeshMap)
    {
        const MeshEntry& Info = Pair.Value;
        if (Info.bPrioritized)
        {
            if (Info.State != LoadState::Completed && Info.State != LoadState::Failed)
            {
                return false; // 아직 처리 중이거나 실패도 아님
            }
        }
    }

    return true;
}
void FFbxManager::LoadFunc()
{
    while (!bStopThread)
    {
        std::unique_lock<std::mutex> Lock(LoadMutex);
        // 비어있으면 Condition Variable을 대기
        LoadCondition.wait(Lock, [&] { 
            return bStopThread || !PriorityLoadQueue.IsEmpty() || !LoadQueue.IsEmpty();
            });

        while (!PriorityLoadQueue.IsEmpty() || !LoadQueue.IsEmpty())
        {
            if (bStopThread)
                break;
            bool IsPriority = !PriorityLoadQueue.IsEmpty();
            TQueue<FString>& Queue = IsPriority ? PriorityLoadQueue : LoadQueue;
            FString FileName;
            if (Queue.Dequeue(FileName))
            {
                {
                    FSpinLockGuard Lock(MeshMapLock);
                    MeshMap[FileName].State = LoadState::Loading;
                }
                UAssetManager::Get().RegisterAsset(FileName, FAssetInfo::LoadState::Loading);

                // 먼저 Binary 파싱을 시도합니다.
                // 이 포인터는 사용이 끝나면 제거합니다.
                FFbxSkeletalMesh* FbxMesh = new FFbxSkeletalMesh();
                TArray<FFbxAnimSequence*> AnimSequences;
                TArray<FFbxAnimStack*> AnimStacks;
                FWString BinaryPath = (FileName + ".bin").ToWideString();
                // Last Modified Time
                int64_t lastModifiedTime = 0;
                if (std::filesystem::exists(FileName.ToWideString()))
                {
                    auto FileTime = std::filesystem::last_write_time(FileName.ToWideString());
                    lastModifiedTime = std::chrono::system_clock::to_time_t(
                        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                            FileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()));

                }

                bool Success = false;
                Success = LoadFBXFromBinary(BinaryPath, lastModifiedTime, FbxMesh, AnimSequences);
                // Binary 파싱 실패. FBX 파싱 시도
                if (!Success)
                {
                    Success = FFbxLoader::ParseFBX(FileName, FbxMesh, AnimSequences, AnimStacks);
                }

                // FBX 파싱 실패
                if (!Success)
                {
                    if (FbxMesh)
                    {
                        delete FbxMesh;
                        FbxMesh = nullptr;
                    }
                    for (auto& Anim : AnimSequences)
                    {
                        if (!Anim) 
                        {
                            delete Anim;
                        }
                    }

                    UE_LOG(ELogLevel::Error, TEXT("Failed to load FBX file: %s"), *FileName);
                    FSpinLockGuard Lock(MeshMapLock);
                    MeshMap[FileName].State = LoadState::Failed;
                    OnLoadFBXFailed.Execute(FileName);
                    continue;
                }

                // Ffbx를 등록
                {
                    FSpinLockGuard Lock(FbxMeshMapLock);
                    FbxMeshMap.Emplace(FileName, FbxMesh);
                }
                // 애니메이션도 마찬가지로 등록  
                {
                    FSpinLockGuard Lock(AnimMapLock);
                    FbxAnimMap.Emplace(FileName, AnimSequences);
                }
                // 변환 큐에 등록
                if (IsPriority)
                {
                    PriorityConvertQueue.Enqueue(FileName);
                }
                else
                {
                    ConvertQueue.Enqueue(FileName);
                }
                ConvertCondition.notify_one();

                // 저장은 메타값만 저장
                {
                    FSpinLockGuard Lock(MetaMapLock);
                    MetaMap[FileName] = BinaryMetaData{ BinaryPath, lastModifiedTime };
                }
            }
        }
    }
    {
        std::lock_guard<std::mutex> Lock(LoadTerminatedMutex);
        bLoadThreadTerminated = true;
    }
    LoadTerminatedCondition.notify_one();
}


void FFbxManager::ConvertFunc()
{
    while (!bStopThread)
    {
        std::unique_lock<std::mutex> Lock(ConvertMutex);
        ConvertCondition.wait(Lock, [&] { 
            return !PriorityConvertQueue.IsEmpty() || !ConvertQueue.IsEmpty() || bStopThread;
            });

        while (!PriorityConvertQueue.IsEmpty() || !ConvertQueue.IsEmpty())
        {
            if (bStopThread)
                break;
            bool IsPriority = !PriorityConvertQueue.IsEmpty();
            TQueue<FString>& Queue = IsPriority ? PriorityConvertQueue : ConvertQueue;
            FString FileName;
            if (Queue.Dequeue(FileName))
            {
                // 변환 큐에서 꺼내서 변환
                FFbxSkeletalMesh* FbxMesh = nullptr;
                bool bContaining = false;
                {
                    FSpinLockGuard Lock(FbxMeshMapLock);
                    bContaining = FbxMeshMap.Contains(FileName);
                    if (!bContaining)
                    {
                        UE_LOG(ELogLevel::Error, TEXT("Unexpected Error: %s"), *FileName);
                    }
                    else
                    {
                        FbxMesh = FbxMeshMap[FileName];
                    }
                }
                if (!bContaining)
                {
                    FSpinLockGuard Lock(MeshMapLock);
                    if (MeshMap.Contains(FileName))
                    {
                        MeshMap[FileName].State = LoadState::Failed;
                    }
                    continue;
                }
                // USkeletalMesh 생성
                USkeletalMesh* SkeletalMesh = nullptr;
                FFbxLoader::GenerateSkeletalMesh(FbxMesh, SkeletalMesh);
                MeshMap[FileName].State = LoadState::Completed;
                MeshMap[FileName].Mesh = SkeletalMesh;

                TArray<FFbxAnimSequence*> FbxSequences = FbxAnimMap[FileName];
                TArray<UAnimSequence*> AnimSequences;
                TArray<FString> AnimNames;

                // 노티파이도 같이 로드
                TMap<FString, TArray<FAnimNotifyEvent>> AnimNameToNotifies;
                FString NotifyFileName = FileName + ".notifies";
                LoadNotifiesFromBinary(StringToWString(*NotifyFileName), AnimNameToNotifies);

                for (const FFbxAnimSequence* FbxSequence : FbxSequences)
                {
                    // 애니메이션을 생성
                    if (FbxSequence)
                    {
                        UAnimSequence* AnimSequence = nullptr;
                        FString AnimName;
                        FFbxLoader::GenerateAnimations(FbxMesh, FbxSequence, AnimName, AnimSequence);

                        if (AnimSequence && AnimNameToNotifies.Contains(AnimName))
                        {
                            AnimSequence->Notifies = AnimNameToNotifies[AnimName];
                        }

                        AnimSequences.Add(AnimSequence);
                        AnimNames.Add(AnimName);
                    }
                }
                {
                    FSpinLockGuard Lock(AnimMapLock);
                    for (UAnimSequence* AnimSeq : AnimSequences)
                    {
                        if (AnimSeq)
                        {
                            AnimMap.Add(AnimSeq->Name + "::" + FileName, {LoadState::Completed, FileName, AnimSeq});
                        }
                    }
                }
                OnLoadFBXCompleted.Execute(FileName);
                UE_LOG(ELogLevel::Display, TEXT("Converted FBX file: %s"), *FileName);

                // 다음 단계에 전달
                SaveQueue.Enqueue(FileName);
                SaveCondition.notify_one();
            }
        }
    }
    {
        std::lock_guard<std::mutex> Lock(ConvertTerminatedMutex);
        bConvertThreadTerminated = true;
    }
    ConvertTerminatedCondition.notify_one();
}

void FFbxManager::SaveFunc()
{
    while (!bStopThread)
    {
        // condition variable을 혼자 갖기위해 lock를 획득
        std::unique_lock<std::mutex> Lock(SaveMutex);
        // 비어있으면 Condition Variable을 대기
        SaveCondition.wait(Lock, [&] { return !SaveQueue.IsEmpty() || bStopThread; });
        while (!SaveQueue.IsEmpty())
        {
            if (bStopThread)
                break;
            FString FileName;
            if (SaveQueue.Dequeue(FileName))
            {
                BinaryMetaData MetaData;
                {
                    FSpinLockGuard Lock(MetaMapLock);
                    if (!MetaMap.Contains(FileName))
                    {
                        UE_LOG(ELogLevel::Error, TEXT("File not found in MetaMap: %s"), *FileName);
                        continue;
                    }
                    MetaData = MetaMap[FileName];
                    MetaMap.Remove(FileName);
                }
                const FWString& BinaryPath = MetaData.BinaryPath;
                const int64_t& lastModifiedTime = MetaData.LastModifiedTime;
                // Binary 파싱
                // Map에서 복사해서 이용
                FFbxSkeletalMesh* Mesh = nullptr;
                {
                    FSpinLockGuard Lock(FbxMeshMapLock);
                    Mesh = FbxMeshMap[FileName];
                }
                TArray<FFbxAnimSequence*> AnimSequences;
                {
                    FSpinLockGuard Lock(AnimMapLock);
                    AnimSequences = FbxAnimMap[FileName];
                }
                if (SaveFBXToBinary(BinaryPath, lastModifiedTime, Mesh, AnimSequences))
                {
                    UE_LOG(ELogLevel::Display, TEXT("Saved FBX to binary: %s"), WStringToString(BinaryPath).c_str());
                }
                else
                {
                    UE_LOG(ELogLevel::Error, TEXT("Failed to save FBX to binary: %s"), WStringToString(BinaryPath).c_str());
                }

                // 더이상 사용되지 않으니 FFbxSkeletalMesh는 제거
                {
                    FSpinLockGuard Lock(FbxMeshMapLock);
                    if (FbxMeshMap.Contains(FileName))
                    {
                        delete FbxMeshMap[FileName];
                        FbxMeshMap[FileName] = nullptr;
                    }
                    else
                    {
                        UE_LOG(ELogLevel::Error, TEXT("File not found in FbxMeshMap: %s"), *FileName);
                    }
                }
            }
        }
    }
    {
        std::lock_guard<std::mutex> Lock(SaveTerminatedMutex);
        bSaveThreadTerminated = true;
    }
    SaveTerminatedCondition.notify_one();
}

// .bin 파일로 저장합니다.
bool FFbxManager::SaveFBXToBinary(const FWString& FilePath, int64_t LastModifiedTime, 
    const FFbxSkeletalMesh* FBXObject, const TArray<FFbxAnimSequence*> FBXSequence)
{
    /** File Open */
    std::ofstream File(FilePath, std::ios::binary);

    if (!File.is_open())
    {
        assert("CAN'T SAVE FBX FILE TO BINARY");
        return false;
    }

    /** Modified */
    File.write(reinterpret_cast<const char*>(&LastModifiedTime), sizeof(&LastModifiedTime));

    /** FBX Name */
    Serializer::WriteFString(File, FBXObject->name);

    /** FBX Mesh */
    uint32 MeshCount = FBXObject->mesh.Num();
    File.write(reinterpret_cast<const char*>(&MeshCount), sizeof(MeshCount));
    for (const FFbxMeshData& MeshData : FBXObject->mesh)
    {
        // Mesh Vertices
        uint32 VertexCount = MeshData.vertices.Num();
        File.write(reinterpret_cast<const char*>(&VertexCount), sizeof(VertexCount));
        if (VertexCount > 0)
        {
            File.write(reinterpret_cast<const char*>(MeshData.vertices.GetData()), sizeof(FFbxVertex) * VertexCount);
        }

        // Mesh Indices
        uint32 IndexCount = MeshData.indices.Num();
        File.write(reinterpret_cast<const char*>(&IndexCount), sizeof(IndexCount));
        if (IndexCount > 0)
        {
            File.write(reinterpret_cast<const char*>(MeshData.indices.GetData()), sizeof(uint32) * IndexCount);
        }

        // Subset
        uint32 SubIndexCount = MeshData.subsetIndex.Num();
        File.write(reinterpret_cast<const char*>(&SubIndexCount), sizeof(SubIndexCount));
        if (SubIndexCount > 0)
        {
            File.write(reinterpret_cast<const char*>(MeshData.subsetIndex.GetData()), sizeof(uint32) * SubIndexCount);
        }

        // Name
        Serializer::WriteFString(File, MeshData.name);
    }

    /** FBX Skeleton */
    uint32 JointCount = FBXObject->skeleton.joints.Num();
    File.write(reinterpret_cast<const char*>(&JointCount), sizeof(JointCount));
    for (const FFbxJoint& Joint : FBXObject->skeleton.joints)
    {
        // Joint Name
        Serializer::WriteFString(File, Joint.name);

        // Parent index
        File.write(reinterpret_cast<const char*>(&Joint.parentIndex), sizeof(Joint.parentIndex));

        // Local bind pose
        File.write(reinterpret_cast<const char*>(&Joint.localBindPose), sizeof(Joint.localBindPose));

        // Inverse bind pose
        File.write(reinterpret_cast<const char*>(&Joint.inverseBindPose), sizeof(Joint.inverseBindPose));

        // Position
        File.write(reinterpret_cast<const char*>(&Joint.position), sizeof(Joint.position));

        // Rotation
        File.write(reinterpret_cast<const char*>(&Joint.rotation), sizeof(Joint.rotation));

        // Scale
        File.write(reinterpret_cast<const char*>(&Joint.scale), sizeof(Joint.scale));
    }

    /** FBX UMaterial */
    uint32 MaterialCount = FBXObject->material.Num();
    File.write(reinterpret_cast<const char*>(&MaterialCount), sizeof(MaterialCount));
    for (UMaterial* const Material : FBXObject->material)
    {
        bool bIsValidMaterial = (Material != nullptr);
        File.write(reinterpret_cast<const char*>(&bIsValidMaterial), sizeof(bIsValidMaterial));

        if (bIsValidMaterial)
        {
            const FMaterialInfo& MaterialInfo = Material->GetMaterialInfo();

            // MaterialInfo.MaterialName (FString)
            Serializer::WriteFString(File, MaterialInfo.MaterialName);

            // MaterialInfo.TextureFlag (uint32)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.TextureFlag), sizeof(MaterialInfo.TextureFlag));

            // MaterialInfo.bTransparent (bool)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.bTransparent), sizeof(MaterialInfo.bTransparent));

            // MaterialInfo.DiffuseColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.DiffuseColor), sizeof(MaterialInfo.DiffuseColor));

            // MaterialInfo.SpecularColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.SpecularColor), sizeof(MaterialInfo.SpecularColor));

            // MaterialInfo.AmbientColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.AmbientColor), sizeof(MaterialInfo.AmbientColor));

            // MaterialInfo.EmissiveColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.EmissiveColor), sizeof(MaterialInfo.EmissiveColor));

            // MaterialInfo.SpecularExponent (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.SpecularExponent), sizeof(MaterialInfo.SpecularExponent));

            // MaterialInfo.IOR (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.IOR), sizeof(MaterialInfo.IOR));

            // MaterialInfo.Transparency (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Transparency), sizeof(MaterialInfo.Transparency));

            // MaterialInfo.BumpMultiplier (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.BumpMultiplier), sizeof(MaterialInfo.BumpMultiplier));

            // MaterialInfo.IlluminanceModel (uint32)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.IlluminanceModel), sizeof(MaterialInfo.IlluminanceModel));

            // MaterialInfo.Metallic (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Metallic), sizeof(MaterialInfo.Metallic));

            // MaterialInfo.Roughness (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Roughness), sizeof(MaterialInfo.Roughness));

            // MaterialInfo.AmbientOcclusion (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.AmbientOcclusion), sizeof(MaterialInfo.AmbientOcclusion));

            // MaterialInfo.ClearCoat (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.ClearCoat), sizeof(MaterialInfo.ClearCoat));

            // MaterialInfo.Sheen (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Sheen), sizeof(MaterialInfo.Sheen));

            // MaterialInfo.TextureInfos (TArray<FTextureInfo>)
            uint32 TextureInfoCount = MaterialInfo.TextureInfos.Num();
            File.write(reinterpret_cast<const char*>(&TextureInfoCount), sizeof(TextureInfoCount));
            for (const FTextureInfo& texInfo : MaterialInfo.TextureInfos)
            {
                Serializer::WriteFString(File, texInfo.TextureName);
                Serializer::WriteFWString(File, texInfo.TexturePath);
                File.write(reinterpret_cast<const char*>(&texInfo.bIsSRGB), sizeof(texInfo.bIsSRGB));
            }
        }
    }

    /** FBX Material Subset */
    uint32 SubsetCount = FBXObject->materialSubsets.Num();
    File.write(reinterpret_cast<const char*>(&SubsetCount), sizeof(SubsetCount));
    for (const FMaterialSubset& Subset : FBXObject->materialSubsets)
    {
        File.write(reinterpret_cast<const char*>(&Subset.IndexStart), sizeof(Subset.IndexStart));
        File.write(reinterpret_cast<const char*>(&Subset.IndexCount), sizeof(Subset.IndexCount));
        File.write(reinterpret_cast<const char*>(&Subset.MaterialIndex), sizeof(Subset.MaterialIndex));
        Serializer::WriteFString(File, Subset.MaterialName);
    }

    /** FBX AABB */
    File.write(reinterpret_cast<const char*>(&FBXObject->AABBmin), sizeof(FVector));
    File.write(reinterpret_cast<const char*>(&FBXObject->AABBmax), sizeof(FVector));

    /** FBX Animations */
    uint32 AnimCount = FBXSequence.Num();
    File.write(reinterpret_cast<const char*>(&AnimCount), sizeof(AnimCount));
    for (const FFbxAnimSequence* AnimSeq : FBXSequence)
    {
        if (AnimSeq)
        {
            // AnimName
            Serializer::WriteFString(File, AnimSeq->Name);
            // Anim Length
            File.write(reinterpret_cast<const char*>(&AnimSeq->Duration), sizeof(AnimSeq->Duration));
            // Anim FrameRate
            File.write(reinterpret_cast<const char*>(&AnimSeq->FrameRate), sizeof(AnimSeq->FrameRate));
            // Anim FrameCount
            File.write(reinterpret_cast<const char*>(&AnimSeq->NumFrames), sizeof(AnimSeq->NumFrames));
            // Anim KeyCount
            File.write(reinterpret_cast<const char*>(&AnimSeq->NumKeys), sizeof(AnimSeq->NumKeys));

            // Anim Tracks
            uint32 TrackCount = AnimSeq->AnimTracks.Num();
            File.write(reinterpret_cast<const char*>(&TrackCount), sizeof(TrackCount));
            for (const FFbxAnimTrack& Track : AnimSeq->AnimTracks)
            {
                // BoneName
                Serializer::WriteFString(File, Track.BoneName);
                // PosKeys
                uint32 PosKeyCount = Track.PosKeys.Num();
                File.write(reinterpret_cast<const char*>(&PosKeyCount), sizeof(PosKeyCount));
                if (PosKeyCount > 0)
                {
                    File.write(reinterpret_cast<const char*>(Track.PosKeys.GetData()), sizeof(FVector) * PosKeyCount);
                }
                // RotKeys
                uint32 RotKeyCount = Track.RotKeys.Num();
                File.write(reinterpret_cast<const char*>(&RotKeyCount), sizeof(RotKeyCount));
                if (RotKeyCount > 0)
                {
                    File.write(reinterpret_cast<const char*>(Track.RotKeys.GetData()), sizeof(FQuat) * RotKeyCount);
                }
                // ScaleKeys
                uint32 ScaleKeyCount = Track.ScaleKeys.Num();
                File.write(reinterpret_cast<const char*>(&ScaleKeyCount), sizeof(ScaleKeyCount));
                if (ScaleKeyCount > 0)
                {
                    File.write(reinterpret_cast<const char*>(Track.ScaleKeys.GetData()), sizeof(FVector) * ScaleKeyCount);
                }
            }
        }
    }
    File.close();
    return true;
}

// .bin 파일을 파싱합니다.
bool FFbxManager::LoadFBXFromBinary(const FWString& FilePath, int64_t LastModifiedTime, 
    FFbxSkeletalMesh* OutFBXObject, TArray<FFbxAnimSequence*>& OutFBXSequence)
{
    UE_LOG(ELogLevel::Display, "Loading binary: %s", WStringToString(FilePath).c_str());
    if (!std::filesystem::exists(FilePath))
    {
        UE_LOG(ELogLevel::Display, "Binary file does not exist: %s", WStringToString(FilePath).c_str());
        return false;
    }

    std::ifstream File(FilePath, std::ios::binary);
    if (!File.is_open())
    {
        UE_LOG(ELogLevel::Warning, "Binary file exists but failed to open: %s", WStringToString(FilePath).c_str());
        return false;
    }

    /** Modified Check */
    int64_t FileLastModifiedTime;
    File.read(reinterpret_cast<char*>(&FileLastModifiedTime), sizeof(FileLastModifiedTime));

    // File is changed.
    if (LastModifiedTime != FileLastModifiedTime)
    {
        UE_LOG(ELogLevel::Display, "Binary file is modified. Parinsg FBX : %s", WStringToString(FilePath).c_str());
        return false;
    }

    TArray<TPair<FWString, bool>> Textures;

    /** FBX Name */
    Serializer::ReadFString(File, OutFBXObject->name);

    /** FBX Mesh */
    uint32 MeshCount;
    File.read(reinterpret_cast<char*>(&MeshCount), sizeof(MeshCount));
    OutFBXObject->mesh.Reserve(MeshCount); // 미리 메모리 할당
    for (uint32 i = 0; i < MeshCount; ++i)
    {
        FFbxMeshData MeshData;

        // Mesh Vertices
        uint32 VertexCount;
        File.read(reinterpret_cast<char*>(&VertexCount), sizeof(VertexCount));
        if (VertexCount > 0)
        {
            MeshData.vertices.SetNum(VertexCount); // 크기 설정
            File.read(reinterpret_cast<char*>(MeshData.vertices.GetData()), sizeof(FFbxVertex) * VertexCount);
        }

        // Mesh Indices
        uint32 IndexCount;
        File.read(reinterpret_cast<char*>(&IndexCount), sizeof(IndexCount));
        if (IndexCount > 0)
        {
            MeshData.indices.SetNum(IndexCount); // 크기 설정
            File.read(reinterpret_cast<char*>(MeshData.indices.GetData()), sizeof(uint32) * IndexCount);
        }

        // Subset
        uint32 SubIndexCount;
        File.read(reinterpret_cast<char*>(&SubIndexCount), sizeof(SubIndexCount));
        if (SubIndexCount > 0)
        {
            MeshData.subsetIndex.SetNum(SubIndexCount); // 크기 설정
            File.read(reinterpret_cast<char*>(MeshData.subsetIndex.GetData()), sizeof(uint32) * SubIndexCount);
        }

        // Name
        Serializer::ReadFString(File, MeshData.name);
        OutFBXObject->mesh.Add(std::move(MeshData));
    }

    /** FBX Skeleton */
    uint32 JointCount;
    File.read(reinterpret_cast<char*>(&JointCount), sizeof(JointCount));
    OutFBXObject->skeleton.joints.Reserve(JointCount); // 미리 메모리 할당
    for (uint32 i = 0; i < JointCount; ++i)
    {
        FFbxJoint Joint;

        // Joint Name
        Serializer::ReadFString(File, Joint.name);

        // Parent index
        File.read(reinterpret_cast<char*>(&Joint.parentIndex), sizeof(Joint.parentIndex));

        // Local bind pose
        File.read(reinterpret_cast<char*>(&Joint.localBindPose), sizeof(Joint.localBindPose));

        // Inverse bind pose
        File.read(reinterpret_cast<char*>(&Joint.inverseBindPose), sizeof(Joint.inverseBindPose));

        // Position
        File.read(reinterpret_cast<char*>(&Joint.position), sizeof(Joint.position));

        // Rotation
        File.read(reinterpret_cast<char*>(&Joint.rotation), sizeof(Joint.rotation));

        // Scale
        File.read(reinterpret_cast<char*>(&Joint.scale), sizeof(Joint.scale));

        OutFBXObject->skeleton.joints.Add(std::move(Joint));
    }

    /** FBX UMaterial */
    uint32 MaterialCount;
    File.read(reinterpret_cast<char*>(&MaterialCount), sizeof(MaterialCount));
    OutFBXObject->material.Reserve(MaterialCount); // 미리 메모리 할당
    for (uint32 i = 0; i < MaterialCount; ++i)
    {
        bool bIsValidMaterial;
        File.read(reinterpret_cast<char*>(&bIsValidMaterial), sizeof(bIsValidMaterial));

        if (bIsValidMaterial)
        {
            UMaterial* NewMaterial = new UMaterial(); // UMaterial 객체 생성
            FMaterialInfo MaterialInfo;

            // MaterialInfo.MaterialName (FString)
            Serializer::ReadFString(File, MaterialInfo.MaterialName);

            // MaterialInfo.TextureFlag (uint32)
            File.read(reinterpret_cast<char*>(&MaterialInfo.TextureFlag), sizeof(MaterialInfo.TextureFlag));

            // MaterialInfo.bTransparent (bool)
            File.read(reinterpret_cast<char*>(&MaterialInfo.bTransparent), sizeof(MaterialInfo.bTransparent));

            // MaterialInfo.DiffuseColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.DiffuseColor), sizeof(MaterialInfo.DiffuseColor));

            // MaterialInfo.SpecularColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.SpecularColor), sizeof(MaterialInfo.SpecularColor));

            // MaterialInfo.AmbientColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.AmbientColor), sizeof(MaterialInfo.AmbientColor));

            // MaterialInfo.EmissiveColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.EmissiveColor), sizeof(MaterialInfo.EmissiveColor));

            // MaterialInfo.SpecularExponent (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.SpecularExponent), sizeof(MaterialInfo.SpecularExponent));

            // MaterialInfo.IOR (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.IOR), sizeof(MaterialInfo.IOR));

            // MaterialInfo.Transparency (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Transparency), sizeof(MaterialInfo.Transparency));

            // MaterialInfo.BumpMultiplier (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.BumpMultiplier), sizeof(MaterialInfo.BumpMultiplier));

            // MaterialInfo.IlluminanceModel (uint32)
            File.read(reinterpret_cast<char*>(&MaterialInfo.IlluminanceModel), sizeof(MaterialInfo.IlluminanceModel));

            // MaterialInfo.Metallic (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Metallic), sizeof(MaterialInfo.Metallic));

            // MaterialInfo.Roughness (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Roughness), sizeof(MaterialInfo.Roughness));

            // MaterialInfo.AmbientOcclusion (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.AmbientOcclusion), sizeof(MaterialInfo.AmbientOcclusion));

            // MaterialInfo.ClearCoat (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.ClearCoat), sizeof(MaterialInfo.ClearCoat));

            // MaterialInfo.Sheen (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Sheen), sizeof(MaterialInfo.Sheen));

            // MaterialInfo.TextureInfos (TArray<FTextureInfo>)
            uint32 TextureInfoCount;
            File.read(reinterpret_cast<char*>(&TextureInfoCount), sizeof(TextureInfoCount));
            MaterialInfo.TextureInfos.Reserve(TextureInfoCount); // 미리 메모리 할당
            for (uint32 j = 0; j < TextureInfoCount; ++j)
            {
                FTextureInfo TexInfo;
                Serializer::ReadFString(File, TexInfo.TextureName);
                Serializer::ReadFWString(File, TexInfo.TexturePath);
                File.read(reinterpret_cast<char*>(&TexInfo.bIsSRGB), sizeof(TexInfo.bIsSRGB));
                Textures.AddUnique({ TexInfo.TexturePath, TexInfo.bIsSRGB });
                MaterialInfo.TextureInfos.Add(std::move(TexInfo));
            }
            NewMaterial->SetMaterialInfo(MaterialInfo);
            OutFBXObject->material.Add(NewMaterial);
        }
        else
        {
            OutFBXObject->material.Add(nullptr);
        }
    }

    /** FBX Material Subset */
    uint32 SubsetCount;
    File.read(reinterpret_cast<char*>(&SubsetCount), sizeof(SubsetCount));
    OutFBXObject->materialSubsets.Reserve(SubsetCount); // 미리 메모리 할당
    for (uint32 i = 0; i < SubsetCount; ++i)
    {
        FMaterialSubset Subset;
        File.read(reinterpret_cast<char*>(&Subset.IndexStart), sizeof(Subset.IndexStart));
        File.read(reinterpret_cast<char*>(&Subset.IndexCount), sizeof(Subset.IndexCount));
        File.read(reinterpret_cast<char*>(&Subset.MaterialIndex), sizeof(Subset.MaterialIndex));
        Serializer::ReadFString(File, Subset.MaterialName);
        OutFBXObject->materialSubsets.Add(std::move(Subset));
    }

    /** FBX AABB */
    File.read(reinterpret_cast<char*>(&OutFBXObject->AABBmin), sizeof(FVector));
    File.read(reinterpret_cast<char*>(&OutFBXObject->AABBmax), sizeof(FVector));

    /** FBX Animations */ 
    uint32 AnimCount;
    File.read(reinterpret_cast<char*>(&AnimCount), sizeof(AnimCount));
    OutFBXSequence.SetNum(AnimCount); // 미리 메모리 할당
    for (uint32 i = 0; i < AnimCount; ++i)
    {
        FFbxAnimSequence* AnimSeq = new FFbxAnimSequence();
        // AnimName
        Serializer::ReadFString(File, AnimSeq->Name);
        // Anim Length
        File.read(reinterpret_cast<char*>(&AnimSeq->Duration), sizeof(AnimSeq->Duration));
        // Anim FrameRate
        File.read(reinterpret_cast<char*>(&AnimSeq->FrameRate), sizeof(AnimSeq->FrameRate));
        // Anim FrameCount
        File.read(reinterpret_cast<char*>(&AnimSeq->NumFrames), sizeof(AnimSeq->NumFrames));
        // Anim KeyCount
        File.read(reinterpret_cast<char*>(&AnimSeq->NumKeys), sizeof(AnimSeq->NumKeys));
        // Anim Tracks
        uint32 TrackCount;
        File.read(reinterpret_cast<char*>(&TrackCount), sizeof(TrackCount));
        OutFBXSequence[i] = AnimSeq;
        OutFBXSequence[i]->AnimTracks.SetNum(TrackCount);
        for (uint32 j = 0; j < TrackCount; ++j)
        {
            FFbxAnimTrack Track;
            // BoneName
            Serializer::ReadFString(File, Track.BoneName);
            // PosKeys
            uint32 PosKeyCount;
            File.read(reinterpret_cast<char*>(&PosKeyCount), sizeof(PosKeyCount));
            if (PosKeyCount > 0)
            {
                Track.PosKeys.SetNum(PosKeyCount); // 크기 설정
                File.read(reinterpret_cast<char*>(Track.PosKeys.GetData()), sizeof(FVector) * PosKeyCount);
            }
            // RotKeys
            uint32 RotKeyCount;
            File.read(reinterpret_cast<char*>(&RotKeyCount), sizeof(RotKeyCount));
            if (RotKeyCount > 0)
            {
                Track.RotKeys.SetNum(RotKeyCount); // 크기 설정
                File.read(reinterpret_cast<char*>(Track.RotKeys.GetData()), sizeof(FQuat) * RotKeyCount);
            }
            // ScaleKeys
            uint32 ScaleKeyCount;
            File.read(reinterpret_cast<char*>(&ScaleKeyCount), sizeof(ScaleKeyCount));
            if (ScaleKeyCount > 0)
            {
                Track.ScaleKeys.SetNum(ScaleKeyCount); // 크기 설정
                File.read(reinterpret_cast<char*>(Track.ScaleKeys.GetData()), sizeof(FVector) * ScaleKeyCount);
            }
            OutFBXSequence[i]->AnimTracks[j] = std::move(Track);
        }
    }

    File.close();

    // Texture load
    if (Textures.Num() > 0)
    {
        for (const TPair<FWString, bool>& Texture : Textures)
        {
            if (FEngineLoop::ResourceManager.GetTexture(Texture.Key) == nullptr)
            {
                FEngineLoop::ResourceManager.LoadTextureFromFile(FEngineLoop::GraphicDevice.Device, Texture.Key.c_str(), Texture.Value);
            }
        }
    }
    UE_LOG(ELogLevel::Display, "Binary loaded : %s", WStringToString(FilePath).c_str());

    return true;
}

bool FFbxManager::SaveNotifiesToBinary(const FWString& FilePath, const TMap<FString, TArray<FAnimNotifyEvent>>& AnimationNotifiesMap)
{
    bool bEmpty = false;

    for (const auto& Pair : AnimationNotifiesMap)
    {
        if (Pair.Value.Num() > 0)
        {
            bEmpty = true;
            break;
        }
    }

    std::ofstream File(FilePath, std::ios::binary);
    if (!File.is_open())
        return false;

    // 1. 애니메이션 개수 저장
    uint32 AnimCount = AnimationNotifiesMap.Num();
    File.write(reinterpret_cast<const char*>(&AnimCount), sizeof(AnimCount));

    for (const auto& Pair : AnimationNotifiesMap)
    {
        // 2. 애니메이션 이름 저장
        Serializer::WriteFString(File, *Pair.Key);

        // 3. 노티파이 배열 직렬화
        const TArray<FAnimNotifyEvent>& Notifies = Pair.Value;
        uint32 NotifyCount = Notifies.Num();
        File.write(reinterpret_cast<const char*>(&NotifyCount), sizeof(NotifyCount));
        for (const FAnimNotifyEvent& Notify : Notifies)
        {
            FString NotifyString = Notify.ToString();
            Serializer::WriteFString(File, NotifyString);
        }
    }

    File.close();
    return true;
}

bool FFbxManager::LoadNotifiesFromBinary(const FWString& FilePath, TMap<FString, TArray<FAnimNotifyEvent>>& AnimationNotifiesMap)
{
    AnimationNotifiesMap.Empty();

    if (!std::filesystem::exists(FilePath))
        return false;

    std::ifstream File(FilePath, std::ios::binary);
    if (!File.is_open())
        return false;

    // 1. 애니메이션 개수 읽기
    uint32 AnimCount = 0;
    File.read(reinterpret_cast<char*>(&AnimCount), sizeof(AnimCount));

    for (uint32 i = 0; i < AnimCount; ++i)
    {
        // 2. 애니메이션 이름 읽기
        FString AnimName;
        Serializer::ReadFString(File, AnimName);

        // 3. 노티파이 배열 읽기
        uint32 NotifyCount = 0;
        File.read(reinterpret_cast<char*>(&NotifyCount), sizeof(NotifyCount));
        TArray<FAnimNotifyEvent> Notifies;
        Notifies.Reserve(NotifyCount);

        for (uint32 j = 0; j < NotifyCount; ++j)
        {
            FString NotifyString;
            Serializer::ReadFString(File, NotifyString);
            FAnimNotifyEvent Notify;
            Notify.InitFromString(NotifyString);
            Notifies.Add(Notify);
        }
        AnimationNotifiesMap.Add(AnimName, Notifies);
    }

    File.close();
    return true;
}
