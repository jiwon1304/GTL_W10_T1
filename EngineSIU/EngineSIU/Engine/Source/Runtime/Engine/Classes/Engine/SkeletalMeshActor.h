#pragma once
#include "GameFramework/Actor.h"

class USkeletalMeshComponent;


class ASkeletalMeshActor : public AActor
{
    DECLARE_CLASS(ASkeletalMeshActor, AActor)

public:
    ASkeletalMeshActor();

    USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }

protected:
    USkeletalMeshComponent* SkeletalMeshComponent;
};
