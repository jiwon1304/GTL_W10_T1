#include "PointLightActor.h"
#include "Components/Light/PointLightComponent.h"
#include "Components/BillboardComponent.h"

APointLight::APointLight()
{
    PointLightComponent = AddComponent<UPointLightComponent>("PointLightComponent_0");

    RootComponent = PointLightComponent;
}

APointLight::~APointLight()
{
}
