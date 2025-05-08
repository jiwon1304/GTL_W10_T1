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
    (UPointLightComponent*, PointLightComponent, = nullptr);
};

