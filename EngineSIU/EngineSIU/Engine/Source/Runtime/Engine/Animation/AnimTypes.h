#pragma once
#include "Container/Array.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "UObject/ObjectMacros.h"

struct FAnimNotifyEvent
{
    float TriggerTime;
    float Duration;
    FName NotifyName;
};

/**
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
};
