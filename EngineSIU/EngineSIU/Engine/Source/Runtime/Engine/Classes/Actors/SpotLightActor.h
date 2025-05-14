#pragma once
#include "LightActor.h"
class ASpotLight :
    public ALight
{
    DECLARE_CLASS(ASpotLight, ALight)
public:
    ASpotLight();
    virtual ~ASpotLight();
protected:
    UPROPERTY
    (None, USpotLightComponent*, SpotLightComponent, = nullptr);
};

