#include "SpotLightActor.h"
#include "Components/Light/SpotLightComponent.h"
#include "Components/BillboardComponent.h"
ASpotLight::ASpotLight()
{
    SpotLightComponent = AddComponent<USpotLightComponent>("USpotLightComponent_0");

    RootComponent = SpotLightComponent;
}

ASpotLight::~ASpotLight()
{
}
