#pragma once
#include "GameFramework/Actor.h"

class UStaticMeshComponent;
class UBoxComponent;

class ATriggerBox : public AActor
{
    DECLARE_CLASS(ATriggerBox, AActor)

public:
    ATriggerBox();
    virtual ~ATriggerBox() override = default;

    virtual void PostSpawnInitialize() override;
    
    virtual void BeginPlay() override;
    
    virtual void Tick(float DeltaTime) override;

protected:

    UPROPERTY
    (None, UBoxComponent*, BoxComponent, = nullptr)

    UPROPERTY
    (None, UStaticMeshComponent*, MeshComponent, = nullptr)

    void ActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);
};
