#pragma once
#include "Pawn.h"

class UCapsuleComponent;
class USkeletalMeshComponent;

struct FAnimNotifyEvent;
class ACharacter : public APawn
{
    DECLARE_CLASS(ACharacter, APawn);
public:
    ACharacter();
    virtual void PostSpawnInitialize() override;
    virtual UObject* Duplicate(UObject* InOuter) override;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    void HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent) const;


    float Velocity;
private:
    USkeletalMeshComponent* Mesh;
    UCapsuleComponent* CapsuleComponent;
};
