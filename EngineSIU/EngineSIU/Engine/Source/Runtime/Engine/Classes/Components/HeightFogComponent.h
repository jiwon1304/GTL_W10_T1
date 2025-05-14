#pragma once
#include "PrimitiveComponent.h"
#include <Math/Color.h>

class UHeightFogComponent : public UPrimitiveComponent
{
    DECLARE_CLASS(UHeightFogComponent, UPrimitiveComponent)
private:
    UPROPERTY(EditAnywhere, FLinearColor, FogInscatteringColor)
    UPROPERTY(EditAnywhere, float, FogDensity)
    UPROPERTY(EditAnywhere, float, FogHeightFalloff)
    UPROPERTY(EditAnywhere, float, StartDistance)
    UPROPERTY(EditAnywhere, float, FogDistanceWeight)
    UPROPERTY(EditAnywhere, float, EndDistance)
    //float FogDensity;
    //float FogHeightFalloff;
    //float StartDistance;
    //float FogDistanceWeight;
    //float EndDistance;
    //FLinearColor FogInscatteringColor;

public:
    UHeightFogComponent(float Density = 0.5f, float HeightFalloff = 0.05f, float StartDist = 0.f, float EndDist = 0.1f, float DistanceWeight = 0.75f);

    float GetFogDensity() { return FogDensity; }
    float GetFogHeightFalloff() { return FogHeightFalloff; }
    float GetStartDistance() { return StartDistance; }
    float GetFogDistanceWeight() { return FogDistanceWeight; }
    float GetEndDistance() { return EndDistance; }
    FLinearColor GetFogColor() { return FogInscatteringColor; }
    
    void SetFogDensity(float value);
    void SetFogHeightFalloff(float value);
    void SetStartDistance(float value);
    void SetFogDistanceWeight(float value);
    void SetEndDistance(float value);
    void SetFogColor(FLinearColor color);

    virtual UObject* Duplicate(UObject* InOuter) override;
    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;
    
};
