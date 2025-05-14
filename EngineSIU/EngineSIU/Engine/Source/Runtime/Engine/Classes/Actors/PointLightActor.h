#pragma once
#include "LightActor.h"
class APointLight :
    public ALight
{
    DECLARE_CLASS(APointLight, ALight)
public:
    APointLight();
    virtual ~APointLight();
protected:
    UPROPERTY
    (None, UPointLightComponent*, PointLightComponent, = nullptr);
};

