#pragma once
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

class AGoalPlatformActor : public AActor
{
    DECLARE_CLASS(AGoalPlatformActor, AActor)

public:
    AGoalPlatformActor();
    virtual ~AGoalPlatformActor() override = default;

protected:
    UPROPERTY
    (None, UBoxComponent*, BoxComponent, = nullptr)

    UPROPERTY
    (None, UStaticMeshComponent*, MeshComponent, = nullptr)
};
