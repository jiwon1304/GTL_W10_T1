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
    FString NotifyString = NotifyName.ToString();
    TMap<FString, FString> NotifyStateProperties;
    NotifyStateClass->GetProperties(NotifyStateProperties);
    FString NotifyStateString = NotifyStateProperties.ToString();
    
    return FString::Printf(TEXT("TrackIndex=%d TriggerTime=%.3f TriggerTimeOffset=%3.f EndTriggerTimeOffset=%.3f Duration=%.3f NotifyName=%s NotifyStateClass=%s"),
        TrackIndex, TriggerTime, TriggerTimeOffset, EndTriggerTimeOffset, Duration, NotifyString, NotifyStateString);
}

bool FAnimNotifyEvent::InitFromString(const FString& InSourceString)
{
    FString NotifyStateString;
    const bool bSuccessful = FParse::Value(*InSourceString, TEXT("TrackIndex="), TrackIndex) ||
        FParse::Value(*InSourceString, TEXT("TriggerTime="), TriggerTime) ||
        FParse::Value(*InSourceString, TEXT("Duration="), Duration) ||
        FParse::Value(*InSourceString, TEXT("TriggerTimeOffset="), TriggerTimeOffset) ||
        FParse::Value(*InSourceString, TEXT("EndTriggerTimeOffset="), EndTriggerTimeOffset) ||
        FParse::Value(*InSourceString, TEXT("NotifyName="), NotifyName);

    FString NotifyStateClassMarker = TEXT("NotifyStateClass=");
    int32 NotifyStateClassStart = InSourceString.Find(NotifyStateClassMarker, ESearchCase::IgnoreCase);
    if (NotifyStateClassStart != INDEX_NONE)
    {
        if (!NotifyStateClass)
        {
            NotifyStateClass = FObjectFactory::ConstructObject<UAnimNotifyState>(nullptr);
        }
        int32 NotifyStateClassEnd = InSourceString.Len();
        NotifyStateString = InSourceString.Mid(NotifyStateClassStart + NotifyStateClassMarker.Len(), NotifyStateClassEnd - NotifyStateClassStart - NotifyStateClassMarker.Len());

        TMap<FString, FString> NotifyStateProperties;
        NotifyStateProperties.InitFromString(NotifyStateString);
        NotifyStateClass->SetProperties(NotifyStateProperties);
    }

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
