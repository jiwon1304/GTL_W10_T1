#include "Character.h"

#include "SoundManager.h"
#include "SpringArmComponent.h"
#include "Animation/AnimTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/FbxManager.h"

ACharacter::ACharacter()
{
}

void ACharacter::PostSpawnInitialize()
{
    Super::PostSpawnInitialize();

    CapsuleComponent = AddComponent<UCapsuleComponent>(FName("CapsuleComponent_0"));
    RootComponent = CapsuleComponent;

    Mesh = AddComponent<USkeletalMeshComponent>(FName("SkeletalMeshComponent_0"));
    Mesh->SetupAttachment(RootComponent);
    USkeletalMesh* SkeletalMesh = FFbxManager::GetSkeletalMesh(L"Cloud_Idle.fbx");
    if (SkeletalMesh)
    {
        Mesh->SetSkeletalMesh(SkeletalMesh);
    }

    USpringArmComponent* SpringArmComp = AddComponent<USpringArmComponent>(FName("SpringArmComponent_0"));
    SpringArmComp->SetWorldRotation(FRotator(0.f, 0.f, 0.f));
    SpringArmComp->SetupAttachment(CapsuleComponent);

    UCameraComponent* CameraComp = AddComponent<UCameraComponent>(FName("CameraComponent_0"));
    CameraComp->SetupAttachment(SpringArmComp);

    SetActorTickInEditor(false);
}

UObject* ACharacter::Duplicate(UObject* InOuter)
{
    ThisClass* NewActor = Cast<ThisClass>(Super::Duplicate(InOuter));
    return NewActor;
}

void ACharacter::BeginPlay()
{
    //Super::BeginPlay();
}

void ACharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ACharacter::HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent) const 
{
    if (NotifyEvent.NotifyName == FName("NONE"))
    {
        FSoundManager::GetInstance().PlaySound("sizzle");
        return;
    }
    else if (NotifyEvent.NotifyName == FName("WALK"))
    {
        FSoundManager::GetInstance().PlaySound("fishdream");
        return;
    }
    //switch (NotifyEvent.NotifyName)
    //{
    //case FName("FIRE"):
    //    FSoundManager::GetInstance().PlaySound("sizzle");
    //    break;
    //case FName("EXPLOSION"):
    //    FSoundManager::GetInstance().PlaySound("fishdream");
    //    break;
    //default:
    //    break;
    //}
}

