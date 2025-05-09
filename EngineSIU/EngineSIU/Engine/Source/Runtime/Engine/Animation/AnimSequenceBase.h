#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class UAnimSequenceBase : public UObject
{
    DECLARE_CLASS(UAnimSequenceBase, UObject)

public:
    UAnimSequenceBase() = default;
    virtual UObject* Duplicate(UObject* InOuter) override;
    // TODO : Add Animation Data
    // TODO : Add Animation Notify
    // TODO : Add Animation Curves
    // TODO : Add Animation Compression

    TArray<struct FAnimNotifyEvent> Notifies;

    float SequenceLength;
    float RateScale;
    bool bLoop;
    //struct FRawCurveTracks RawCurveData;
};

