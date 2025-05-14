#pragma once

#include "FbxObject.h"
#include "Container/Array.h"
#include "Container/Map.h"
#include "Container/String.h"
#include "Container/Queue.h"
#include "Asset/SkeletalMeshAsset.h"
#include <mutex>

#include "Animation/AnimSequence.h"
#include "Delegates/DelegateCombination.h"
#include "Core/Misc/Spinlock.h"

class USkeletalMesh;

//DECLARE_DELEGATE_OneParam(FOnLoadFBXStarted, const FString& /*filename*/);
DECLARE_DELEGATE_OneParam(FOnLoadFBXCompleted, const FString& /*filename*/);
DECLARE_DELEGATE_OneParam(FOnLoadFBXFailed, const FString& /*filename*/);

// Skeletal Mesh를 관리하는 구조체
struct FFbxManager
{
public:
    enum class LoadState : uint8
    {
        //Prioritized,
        Queued,
        Loading,
        Completed,
        Failed
    };
    struct MeshEntry {
        LoadState State;
        bool bPrioritized = false;
        USkeletalMesh* Mesh;
    };
    struct AnimEntry
    {
        LoadState State; // 항상 Completed가 될것임
        FString FileName; // SkeletalMesh의 파일이름
        UAnimSequence* Sequence;
    };

    inline static FSpinLock MeshMapLock; // MeshEntry의 Map에 접근할 때 쓰는 스핀락 : map 접근에는 mutex사용안함
    inline static FSpinLock AnimMapLock; // AnimEntry의 Map에 접근할 때 쓰는 스핀락
public:
    static void Init();
    static void Shutdown();
    static void Load(const FString& filename, bool bPrioritized = false);

    static USkeletalMesh* GetSkeletalMesh(const FString& filename);
    static UAnimSequence* GetAnimSequenceByName(const FString& SeqName);
    static const TMap<FString, MeshEntry>& GetSkeletalMeshes();
    static const TMap<FString, AnimEntry>& GetAnimSequences();
    static bool IsPriorityQueueDone();

    // UAssetManager와 연동
    inline static FOnLoadFBXCompleted OnLoadFBXCompleted;
    inline static FOnLoadFBXFailed OnLoadFBXFailed;

private:
    static void LoadFunc();
    static void ConvertFunc();
    static void SaveFunc();

    static bool SaveFBXToBinary(const FWString& FilePath, int64_t LastModifiedTime,
        const FFbxSkeletalMesh* FBXObject, const TArray<FFbxAnimSequence*> FBXSequence);
    static bool LoadFBXFromBinary(const FWString& FilePath, int64_t LastModifiedTime,
        FFbxSkeletalMesh* OutFBXObject, TArray<FFbxAnimSequence*>& OutFBXSequence);

    static bool SaveNotifiesToBinary(const FWString& FilePath, const TMap<FString, TArray<FAnimNotifyEvent>>& AnimationNotifiesMap);
    static bool LoadNotifiesFromBinary(const FWString& FilePath, TMap<FString, TArray<FAnimNotifyEvent>>& AnimationNotifiesMap);

    inline static TMap<FString, MeshEntry> MeshMap;
    inline static TMap<FString, AnimEntry> AnimMap; // filename::애니메이션 이름

    /*
    MainThread -> LoadThread -> ConvertThread -> AnimThread -> SaveThread
    데이터는 큐를 통해서 전달.
    */

    ///////////////////////////////////
    // 파이프라인 내부에서 사용되는 변수들
    // FFbxSkeletalMesh 맵 : USkeletalMesh로 변환되고 .bin로 저장되기 전까지 저장
    inline static FSpinLock FbxMeshMapLock; // FFbxSkeletalMesh의 Map에 접근할 때 쓰는 스핀락
    inline static TMap<FString, FFbxSkeletalMesh*> FbxMeshMap;

    // FFbxAnimSequence 맵 : UAnimSequence로 변환되기 전까지 저장.
    inline static FSpinLock FbxAnimMapLock;
    // Key : 파일이름 / Value.Name : 애니메이션 이름
    inline static TMap<FString, TArray<FFbxAnimSequence*>> FbxAnimMap;

    // .fbx / .bin 에서 FFbxSkeletalMesh를 로드합니다.
    inline static TQueue<FString> LoadQueue; // MeshMap을 참조
    inline static TQueue<FString> PriorityLoadQueue; // MeshMap을 참조
    inline static std::thread  LoadThread;
    inline static std::condition_variable LoadCondition;
    inline static std::mutex LoadMutex;

    // FFbxSkeletalMesh -> USkeletalMesh
    inline static TQueue<FString> ConvertQueue; // MeshMap을 참조
    inline static TQueue<FString> PriorityConvertQueue; // MeshMap을 참조
    inline static std::thread  ConvertThread;
    inline static std::condition_variable ConvertCondition;
    inline static std::mutex ConvertMutex;

    // FFbxSkeletalMesh -> .bin
    struct BinaryMetaData
    {
        FWString BinaryPath;
        int64_t LastModifiedTime;
    };
    inline static FSpinLock MetaMapLock; // MetaMap에 접근할 때 쓰는 스핀락
    inline static TMap<FString, BinaryMetaData> MetaMap; // MeshMap을 참조
    inline static TQueue<FString> SaveQueue; // MeshMap을 참조하지 않음
    inline static std::thread  SaveThread;
    inline static std::condition_variable SaveCondition;
    inline static std::mutex SaveMutex;

    // 전역 쓰레드 종료 신호
    inline static bool bStopThread = false;
    inline static std::mutex LoadTerminatedMutex;
    inline static std::condition_variable LoadTerminatedCondition;
    inline static bool bLoadThreadTerminated = false;
    inline static std::mutex ConvertTerminatedMutex;
    inline static std::condition_variable ConvertTerminatedCondition;
    inline static bool bConvertThreadTerminated = false;
    inline static std::mutex SaveTerminatedMutex;
    inline static std::condition_variable SaveTerminatedCondition;
    inline static bool bSaveThreadTerminated = false;
};
