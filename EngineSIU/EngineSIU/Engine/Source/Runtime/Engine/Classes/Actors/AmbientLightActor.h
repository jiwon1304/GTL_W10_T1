#pragma once
#include "LightActor.h"
class AAmbientLight : public ALight
{
    DECLARE_CLASS(AAmbientLight, ALight)
public:
    AAmbientLight();
    virtual ~AAmbientLight();

protected:
    UPROPERTY
    (None, UAmbientLightComponent*, AmbientLightComponent, = nullptr);

    UPROPERTY
    (None, UBillboardComponent*, BillboardComponent, = nullptr);
};
