#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Container/Array.h"
#include "AnimTypes.h"

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

    void GetAnimNotifies(const float& StartTime, const float& DeltaTime, bool bAllowLooping, TArray<FAnimNotifyEvent>& OutActiveNotifies) const;
    void GetAnimNotifiesFromDeltaPositions(const TArray<FAnimNotifyEvent>& Notifies, float PreviousPosition, float CurrentPosition, TArray<FAnimNotifyEvent>& OutNotifies) const;


public:
    FString Name;
    UAnimDataModel* DataModel;

    TArray<FAnimNotifyEvent> Notifies;

    UPROPERTY
    (EditAnywhere, float, SequenceLength)

    UPROPERTY
    (EditAnywhere, float, RateScale)

    UPROPERTY
    (None, bool, bLoop)

    //struct FRawCurveTracks RawCurveData;
};



