#pragma once
#include "Container/Array.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "UObject/ObjectMacros.h"
#include "Core/Misc/Parse.h"


class UAnimNotifyState;
/* [참고] UAnimNotify: 한 프레임에서 한 번만 발생하는 이벤트 */ 
struct FAnimNotifyEvent
{
    // TArray Contains 용도로 추가
    friend bool operator==(const FAnimNotifyEvent& Lhs, const FAnimNotifyEvent& Rhs);
    friend bool operator!=(const FAnimNotifyEvent& Lhs, const FAnimNotifyEvent& Rhs);

    /** using Track UI */ 
    int32 TrackIndex;
    
    float TriggerTime;  // 발생 시간

    /* Notify가 시작되도록 보정된 시간 오프셋 : 너무 늦게 실행되면 조금 더 일찍 실행되도록 */
    float TriggerTimeOffset = 0.f;
    /* Notify 종료 보정을 위한 시간 오프셋 : 종료가 너무 늦거나 빠르게 되는 것을 조절 */
    float EndTriggerTimeOffset = 0.f;

    FName NotifyName;

    UAnimNotifyState* NotifyStateClass = nullptr;

    float Duration;     // 지속 시간 (0이면 단발성) = EndTriggerTime - TriggerTime

    float GetDuration() const;
    float GetTriggerTime() const;
    float GetEndTriggerTime() const;
    bool IsStateNotify() const;

    FString ToString() const
    {
        FString NotifyString = NotifyName.ToString();
        return FString::Printf(TEXT("TrackIndex: %d, TriggerTime: %.3f, TriggerTimeOffset: %3.f, EndTriggerTimeOffset: %.3f Duration: %.3f"), TrackIndex, TriggerTime, TriggerTimeOffset, EndTriggerTimeOffset, Duration)
            + NotifyString;
    }
    bool InitFromString(const FString& InSourceString)
    {
        TriggerTime = 0;
        Duration = 0;
        TrackIndex = 0;
        // The initialization is only successful if the X and Y values can all be parsed from the string
        const bool bSuccessful = FParse::Value(*InSourceString, TEXT("TrackIndex="), TrackIndex) &&
            FParse::Value(*InSourceString, TEXT("TriggerTime="), TriggerTime) &&
            FParse::Value(*InSourceString, TEXT("Duration="), Duration);
        return bSuccessful;
    }
};

/**
* [본 하나에 대한 전체 트랙 : T, R, S, Curve 묶음]
* 하나의 트랙에 대한 원시 키프레임 데이터입니다. 각 배열은 NumFrames 개의 요소 또는 1개의 요소를 포함합니다.
* 모든 키가 동일한 경우, 간단한 압축 방식으로 모든 키를 하나의 키로 줄여 전체 시퀀스에서 일정하게 유지됩니다.
*/
struct FRawAnimSequenceTrack
{
    TArray<FVector> PosKeys;
    TArray<FQuat> RotKeys;
    TArray<FVector> ScaleKeys;

    friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrack& T)
    {
        Ar << T.PosKeys;
        Ar << T.RotKeys;
        Ar << T.ScaleKeys;
        return Ar;
    }

    bool ContainsNaN() const
    {
        bool bContainsNaN = false;

        auto CheckForNan = [&bContainsNaN](const auto& Keys) -> bool
            {
                if (!bContainsNaN)
                {
                    for (const auto& Key : Keys)
                    {
                        if (Key.ContainsNaN()) 
                        {
                            return true;
                        }
                    }

                    return false;
                }

                return true;
            };

        bContainsNaN = CheckForNan(PosKeys);
        bContainsNaN = CheckForNan(RotKeys);
        bContainsNaN = CheckForNan(ScaleKeys);

        return bContainsNaN;
    }

    static constexpr uint32 SingleKeySize = sizeof(FVector) + sizeof(FQuat) + sizeof(FVector);

    FString ToString() const
    {
        FString Result;
        Result += TEXT("PosKeys: ");
        for (const auto& Key : PosKeys)
        {
            Result += Key.ToString() + TEXT(", ");
        }
        Result += TEXT("\nRotKeys: ");
        for (const auto& Key : RotKeys)
        {
            Result += Key.ToString() + TEXT(", ");
        }
        Result += TEXT("\nScaleKeys: ");
        for (const auto& Key : ScaleKeys)
        {
            Result += Key.ToString() + TEXT(", ");
        }
        return Result;
    }


    bool InitFromString(const FString& InString)
    {
        FString Trimmed = InString.TrimStartAndEnd();

        // 각 Key의 시작 위치를 찾음
        int32 PosStart = Trimmed.Find(TEXT("PosKeys:"));
        int32 RotStart = Trimmed.Find(TEXT("RotKeys:"));
        int32 ScaleStart = Trimmed.Find(TEXT("ScaleKeys:"));

        if (PosStart == INDEX_NONE || RotStart == INDEX_NONE || ScaleStart == INDEX_NONE)
        {
            return false;
        }

        // 각 Key의 값 추출
        FString PosPart = Trimmed.Mid(PosStart + 8, RotStart - (PosStart + 8));
        FString RotPart = Trimmed.Mid(RotStart + 8, ScaleStart - (RotStart + 8));
        FString ScalePart = Trimmed.Mid(ScaleStart + 10);

        PosPart = PosPart.TrimStartAndEnd();
        RotPart = RotPart.TrimStartAndEnd();
        ScalePart = ScalePart.TrimStartAndEnd();

        // 각 배열 초기화
        if (!PosKeys.InitFromString(PosPart)) return false;
        if (!RotKeys.InitFromString(RotPart)) return false;
        if (!ScaleKeys.InitFromString(ScalePart)) return false;

        return true;
    }
};


struct FBoneAnimationTrack
{
    FRawAnimSequenceTrack InternalTrackData;
    int32 BoneTreeIndex = INDEX_NONE;
    FName Name;

    FString ToString() const
    {
        return FString::Printf(TEXT("BoneTreeIndex=%d, Name=%s, TrackData=%s"), BoneTreeIndex, *Name.ToString(), *InternalTrackData.ToString());
    }

    bool InitFromString(const FString& InSourceString)
    {
        // BoneTreeIndex와 Name은 FParse::Value 사용 (구분자는 =로 통일)
        if (!FParse::Value(*InSourceString, TEXT("BoneTreeIndex="), BoneTreeIndex))
            return false;

        if (!FParse::Value(*InSourceString, TEXT("Name="), Name))
            return false;

        // TrackData= 뒤의 문자열만 추출
        const FString TrackDataKey = TEXT("TrackData=");
        int32 TrackDataStart = InSourceString.Find(TrackDataKey, ESearchCase::CaseSensitive);
        if (TrackDataStart == INDEX_NONE)
            return false;

        // TrackData= 바로 뒤부터 끝까지
        FString TrackDataString = InSourceString.Mid(TrackDataStart + TrackDataKey.Len());
        TrackDataString = TrackDataString.TrimStartAndEnd();

        // InternalTrackData 파싱
        return InternalTrackData.InitFromString(TrackDataString);
    }


};
