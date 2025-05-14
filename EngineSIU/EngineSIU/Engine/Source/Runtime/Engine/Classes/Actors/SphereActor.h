#pragma once
#include "GameFramework/Actor.h"

class USphereComponent;


class ASphereActor : public AActor
{
    DECLARE_CLASS(ASphereActor, AActor)
public:
    ASphereActor();

    USphereComponent* GetShapeComponent() const;
protected:
    UPROPERTY
    (None, USphereComponent*, SphereComponent, = nullptr);

};

