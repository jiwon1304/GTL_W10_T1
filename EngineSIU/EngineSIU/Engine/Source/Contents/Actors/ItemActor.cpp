
#include "ItemActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/FObjLoader.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/FbxManager.h"

AItemActor::AItemActor()
{
    MeshComponent = AddComponent<USkeletalMeshComponent>(FName("MeshComponent_0"));
    RootComponent = MeshComponent;
}

void AItemActor::PostSpawnInitialize()
{
    AActor::PostSpawnInitialize();
    /*
    MeshComponent = AddComponent<USkeletalMeshComponent>(FName("MeshComponent_0"));
    auto mesh = FFbxLoader::GetFbxObject("Contents/X Bot.fbx");
    MeshComponent->SetSkeletalMesh(mesh);
    // MeshComponent->SetSkeletalMesh(FFbxManager::GetSkeletalMesh(L"Contents/55-rp_nathan_animated_003_walking_fbx/rp_nathan_animated_003_walking.fbx"));
    //MeshComponent->SetSkeletalMesh(FFbxManager::GetSkeletalMesh(L"Contents/hand/girl.fbx"));
    //MeshComponent->SetSkeletalMesh(FFbxManager::GetSkeletalMesh(L"Contents/suzanne.fbx"));
    MeshComponent->SetupAttachment(RootComponent);
    */
}

void AItemActor::BeginPlay()
{
    AActor::BeginPlay();

    SetHidden(false);

    OnActorBeginOverlap.AddLambda(
        [this](AActor* OverlappedActor, AActor* OtherActor)
        {
            ActorBeginOverlap(OverlappedActor, OtherActor);
        }
    );
}

void AItemActor::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);

    ElapsedTime += DeltaTime;

    if (UStaticMeshComponent* MeshComp = GetComponentByClass<UStaticMeshComponent>())
    {
        MeshComp->SetRelativeRotation(FRotator(0.f, ElapsedTime * Speed, 0.f));
        MeshComp->SetRelativeLocation(FVector(0.f, 0.f, FMath::Sin(ElapsedTime * FloatingFrequency) * FloatingHeight));       
    }
}

void AItemActor::Reset()
{
    SetHidden(false);
}

void AItemActor::ActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
    
}
