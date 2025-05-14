#include "AnimTypes.h"
#include "AnimNotifies/AnimNotifyState.h"
#include "UObject/ObjectFactory.h"

float FAnimNotifyEvent::GetDuration() const
{
    return Duration;
}

float FAnimNotifyEvent::GetTriggerTime() const
{
    return TriggerTime + TriggerTimeOffset; // TriggerTime : [0, SequenceLength]
}

float FAnimNotifyEvent::GetEndTriggerTime() const
{
    if (!NotifyStateClass && (EndTriggerTimeOffset != 0.f))
    {
        UE_LOG(ELogLevel::Warning, TEXT("Anim Notify %s is non state, but has an EndTriggerTimeOffset %f!"), *NotifyName.ToString(), EndTriggerTimeOffset);
    }

    /* Notify State인 경우 Duration까지 고려하여 반환 */
    return NotifyStateClass ? (GetTriggerTime() + Duration + EndTriggerTimeOffset) : GetTriggerTime();
}

bool FAnimNotifyEvent::IsStateNotify() const
{
    return NotifyStateClass != nullptr;
}

FString FAnimNotifyEvent::ToString() const
{
    FString NotifyClassName = NotifyStateClass ? NotifyStateClass->GetClass()->StaticClass()->GetName() : TEXT("None");

    FString NotifyString = NotifyName.ToString();
    TMap<FString, FString> NotifyStateProperties;
    if (NotifyStateClass)
    {
        NotifyStateClass->GetProperties(NotifyStateProperties);
    }
    FString NotifyStateString = NotifyStateProperties.ToString();
    
    return FString::Printf(TEXT("Class=%s TrackIndex=%d TriggerTime=%f TriggerTimeOffset=%f EndTriggerTimeOffset=%f Duration=%f NotifyName=%s NotifyStateClass=%s"),
        *NotifyClassName, TrackIndex, TriggerTime, TriggerTimeOffset, EndTriggerTimeOffset, Duration, *NotifyString, *NotifyStateString);
}

bool FAnimNotifyEvent::InitFromString(const FString& InSourceString)
{
    bool bSuccessful = FParse::Value(*InSourceString, TEXT("TrackIndex="), TrackIndex);
    bSuccessful |= FParse::Value(*InSourceString, TEXT("TriggerTime="), TriggerTime);
    bSuccessful |= FParse::Value(*InSourceString, TEXT("Duration="), Duration);
    bSuccessful |= FParse::Value(*InSourceString, TEXT("TriggerTimeOffset="), TriggerTimeOffset);
    bSuccessful |= FParse::Value(*InSourceString, TEXT("EndTriggerTimeOffset="), EndTriggerTimeOffset);
    bSuccessful |= FParse::Value(*InSourceString, TEXT("NotifyName="), NotifyName);

    //FString NotifyStateString;
    FString NotifyStateClassMarker = TEXT("NotifyStateClass=");
    int32 NotifyStateClassStart = InSourceString.Find(NotifyStateClassMarker, ESearchCase::IgnoreCase);
    // 저장안함
    //if (NotifyStateClassStart != INDEX_NONE)
    //{
    //    assert(NotifyStateClass != nullptr);
    //    int32 NotifyStateClassEnd = InSourceString.Len();
    //    NotifyStateString = InSourceString.Mid(NotifyStateClassStart + NotifyStateClassMarker.Len(), NotifyStateClassEnd - NotifyStateClassStart - NotifyStateClassMarker.Len());

    //    TMap<FString, FString> NotifyStateProperties;
    //    NotifyStateProperties.InitFromString(NotifyStateString);
    //    NotifyStateClass->SetProperties(NotifyStateProperties);
    //}

    return bSuccessful;
}

bool operator==(const FAnimNotifyEvent& Lhs, const FAnimNotifyEvent& Rhs)
{
    return Lhs.NotifyName == Rhs.NotifyName &&
        Lhs.TriggerTime == Rhs.TriggerTime &&
        Lhs.Duration == Rhs.Duration &&
        Lhs.EndTriggerTimeOffset == Rhs.EndTriggerTimeOffset &&
        Lhs.TriggerTimeOffset == Rhs.TriggerTimeOffset &&
        Lhs.NotifyStateClass == Rhs.NotifyStateClass;
}

bool operator!=(const FAnimNotifyEvent& Lhs, const FAnimNotifyEvent& Rhs)
{
    return !(Lhs == Rhs);
}
