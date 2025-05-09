#pragma once
#include "Container/Array.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "UObject/ObjectMacros.h"


/**
* Raw keyframe data for one track.Each array will contain either NumFrames elements or 1 element.
* One element is used as a simple compression scheme where if all keys are the same, they'll be
* reduced to 1 key that is constant over the entire sequence.
*/
struct FRawAnimSequenceTrack
{
    /** Position keys. */
    TArray<FVector> PosKeys;

    /** Rotation keys. */
    TArray<FQuat> RotKeys;

    /** Scale keys. */
    TArray<FVector> ScaleKeys;

    // Serializer.
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
