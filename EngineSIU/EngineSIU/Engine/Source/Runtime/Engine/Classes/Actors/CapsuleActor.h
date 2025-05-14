#pragma once
#include "GameFramework/Actor.h"

class UCapsuleComponent;


class ACapsuleActor : public AActor
{
    DECLARE_CLASS(ACapsuleActor, AActor)
public:
    ACapsuleActor();

    UCapsuleComponent* GetShapeComponent() const;
protected:
    UPROPERTY
    (None, UCapsuleComponent*, CapsuleComponent, = nullptr);

};

