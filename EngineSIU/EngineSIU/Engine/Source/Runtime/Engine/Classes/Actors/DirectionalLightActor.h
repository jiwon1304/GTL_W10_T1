#pragma once
#include "LightActor.h"
class ADirectionalLight : public ALight
{
    DECLARE_CLASS(ADirectionalLight, ALight)
    
public:
    ADirectionalLight();
    virtual ~ADirectionalLight();

public:
    void SetIntensity(float Intensity);

protected:
    UPROPERTY
    (UDirectionalLightComponent*, DirectionalLightComponent, = nullptr);
};

