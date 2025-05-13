#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"

class UAnimDataModel;

enum ETypeAdvanceAnim : int
{
    ETAA_Default,
    ETAA_Finished,
    ETAA_Looped
};

class UAnimSequenceBase : public UObject
{
    DECLARE_CLASS(UAnimSequenceBase, UObject)

public:
    UAnimSequenceBase();
    bool IsNotifyAvailable() const;

public:
    void SetName(const FString& InName) { Name = InName; }
    FString GetSeqName() const { return Name; }

    void SetDataModel(UAnimDataModel* InDataModel) { DataModel = InDataModel; }
    UAnimDataModel* GetDataModel() const { return DataModel; }

    void SetSequenceLength(float InLength) { SequenceLength = InLength; }
    float GetPlayLength() const { return SequenceLength; }

    void SetRateScale(float InRateScale) { RateScale = InRateScale; }
    float GetRateScale() const { return RateScale; }

    void SetLooping(bool bIsLooping) { bLoop = bIsLooping; }
    bool IsLooping() const { return bLoop; }

    void GetAnimNotifies(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<const FAnimNotifyEvent*>& OutActiveNotifies) const;
    void GetAnimNotifiesFromDeltaPositions(const TArray<FAnimNotifyEvent>& Notifies, float PreviousPosition, float CurrentPosition, TArray<const FAnimNotifyEvent*>& OutNotifies) const;


public:
    FString Name;
    UAnimDataModel* DataModel = nullptr;

    TArray<struct FAnimNotifyEvent> Notifies;

    UPROPERTY
    (float, SequenceLength)

    UPROPERTY
    (float, RateScale)

    UPROPERTY
    (bool, bLoop)

    //struct FRawCurveTracks RawCurveData;
};



