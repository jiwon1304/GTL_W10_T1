#pragma once
#include "AnimNotifyState.h"

class UAnimNotifyState_SlowMotion : public UAnimNotifyState
{
    DECLARE_CLASS(UAnimNotifyState_SlowMotion, UAnimNotifyState);
public:

    UAnimNotifyState_SlowMotion() = default;
    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, float Duration) override;
    virtual void NotifyTick(USkeletalMeshComponent* MeshComp, float DeltaTime) override;
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp) override;

    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override
    {
        OutProperties.Add(TEXT("UAnimNotifyState_SlowMotion::OriginalTimeDilation"), FString::Printf(TEXT("%f"), OriginalTimeDilation));
        OutProperties.Add(TEXT("UAnimNotifyState_SlowMotion::SlowRate"), FString::Printf(TEXT("%f"), SlowRate));
    }
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override
    {
        const FString* TempStr = nullptr;
        TempStr = InProperties.Find(TEXT("UAnimNotifyState_SlowMotion::OriginalTimeDilation"));
        if (TempStr)
        {
            OriginalTimeDilation = FString::ToFloat(*TempStr);
        }
        TempStr = InProperties.Find(TEXT("UAnimNotifyState_SlowMotion::SlowRate"));
        if (TempStr)
        {
            SlowRate = FString::ToFloat(*TempStr);
        }
    }

private:
    float OriginalTimeDilation = 1.0f; // 기존 시간 배율 저장
    float SlowRate = 0.3f;
};

