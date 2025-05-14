#pragma once
#include "Actor.h"

class APawn : public AActor
{
    DECLARE_CLASS(APawn, AActor)
public:
    APawn() = default;
    virtual UObject* Duplicate(UObject* InOuter) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

};

