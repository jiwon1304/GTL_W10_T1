#pragma once
#include"GameFramework/Actor.h"

class UProjectileMovementComponent;
class UPointLightComponent;
class USphereComp;

class AFireballActor : public AActor
{
    DECLARE_CLASS(AFireballActor, AActor)
public:
    AFireballActor();
    virtual ~AFireballActor();
    virtual void BeginPlay() override;

protected:
    
    UPROPERTY
    (None, UProjectileMovementComponent*, ProjectileMovementComponent, = nullptr);
   
    UPROPERTY
    (None, UPointLightComponent*, PointLightComponent, = nullptr);
   
    UPROPERTY
    (None, USphereComp*, SphereComp, = nullptr);

};
