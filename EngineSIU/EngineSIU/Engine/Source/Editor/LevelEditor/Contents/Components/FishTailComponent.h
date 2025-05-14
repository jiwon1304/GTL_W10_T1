#pragma once
#include "Components/StaticMeshComponent.h"

class UFishTailComponent : public UStaticMeshComponent
{
    DECLARE_CLASS(UFishTailComponent, UStaticMeshComponent)

public:
    UFishTailComponent();
    virtual ~UFishTailComponent() override = default;

    virtual void TickComponent(float DeltaTime) override;

    virtual void InitializeComponent() override;

    void SetCurrentFrequency(float InFrequency) { CurrentFrequency = FMath::Max(0.f, FMath::Min(InFrequency, MaxFrequency)); }
    float GetCurrentFrequency() const { return CurrentFrequency; }

    float GetMaxFrequency() const { return MaxFrequency; }

    void SetCurrentYaw(float InMaxYaw) { CurrentYaw = FMath::Max(0.f, FMath::Min(InMaxYaw, MaxYaw)); }
    float GetCurrentYaw() const { return CurrentYaw; }

    float GetMaxYaw() const { return MaxYaw; }

protected:
    float MaxFrequency = 10.f;

    float CurrentFrequency = 10.f;

    float ElapsedTime = 0.f;

    float MaxYaw = 50.f;

    float CurrentYaw = 50.f;
};
