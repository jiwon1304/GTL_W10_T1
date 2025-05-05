#pragma once
#include "GameFramework/Actor.h"

class UStaticMeshComponent;
class USphereComponent;
class USkinnedMeshComponent;

class AItemActor : public AActor
{
    DECLARE_CLASS(AItemActor, AActor)

public:
    AItemActor();
    virtual ~AItemActor() override = default;

    virtual void PostSpawnInitialize() override;
    
    virtual void BeginPlay() override;
    
    virtual void Tick(float DeltaTime) override;

    void Reset();

protected:
    float Speed = 250.f;

    float FloatingFrequency = 3.f;

    float FloatingHeight = 0.3f;

    float ElapsedTime = 0.f;

    UPROPERTY
    (USphereComponent*, SphereComponent, = nullptr)

    UPROPERTY
    (USkinnedMeshComponent*, MeshComponent, = nullptr)

    void ActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);
};
