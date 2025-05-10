#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"

class UAnimDataModel;

class UAnimSequenceBase : public UObject
{
    DECLARE_CLASS(UAnimSequenceBase, UObject)

public:
    UAnimSequenceBase();

public:
    void SetName(const FString& InName) { Name = InName; }
    FString GetSeqName() const { return Name; }

    void SetDataModel(UAnimDataModel* InDataModel) { DataModel = InDataModel; }
    UAnimDataModel* GetDataModel() const { return DataModel; }

    void SetSequenceLength(float InLength) { SequenceLength = InLength; }
    float GetSequenceLength() const { return SequenceLength; }

    void SetRateScale(float InRateScale) { RateScale = InRateScale; }
    float GetRateScale() const { return RateScale; }

    void SetLooping(bool bIsLooping) { bLoop = bIsLooping; }
    bool IsLooping() const { return bLoop; }



public:
    FString Name;
    UAnimDataModel* DataModel;

    TArray<struct FAnimNotifyEvent> Notifies;

    UPROPERTY
    (float, SequenceLength)

    UPROPERTY
    (float, RateScale)

    UPROPERTY
    (bool, bLoop)

    //struct FRawCurveTracks RawCurveData;
};

inline UAnimSequenceBase::UAnimSequenceBase()
{
    RateScale = 1.0f;
}

