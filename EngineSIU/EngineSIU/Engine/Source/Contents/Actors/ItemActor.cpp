
#include "ItemActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/FObjLoader.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/FFbxLoader.h"

AItemActor::AItemActor()
{
    // FFbxManager::CreateSkeletalMesh(L"Contents/55-rp_nathan_animated_003_walking_fbx/rp_nathan_animated_003_walking.fbx");
    //FFbxManager::CreateSkeletalMesh(L"Contents/suzanne.fbx");
    //FFbxManager::CreateSkeletalMesh(L"Contents/girl.fbx");

}

void AItemActor::PostSpawnInitialize()
{
    AActor::PostSpawnInitialize();

    SphereComponent = AddComponent<USphereComponent>(FName("SphereComponent_0"));
    SetRootComponent(SphereComponent);

    MeshComponent = AddComponent<USkeletalMeshComponent>(FName("MeshComponent_0"));
    const FString Path = "./Contents/55-rp_nathan_animated_003_walking_fbx/rp_nathan_animated_003_walking.fbx";
    USkeletalMesh* BinMesh = FFbxLoader::LoadBinaryObject(Path);
    USkeletalMesh* mesh = BinMesh ? BinMesh : FFbxLoader::GetFbxObject(Path);
    MeshComponent->SetSkeletalMesh(mesh);
    // MeshComponent->SetSkeletalMesh(FFbxManager::GetSkeletalMesh(L"Contents/55-rp_nathan_animated_003_walking_fbx/rp_nathan_animated_003_walking.fbx"));
    //MeshComponent->SetSkeletalMesh(FFbxManager::GetSkeletalMesh(L"Contents/hand/girl.fbx"));
    //MeshComponent->SetSkeletalMesh(FFbxManager::GetSkeletalMesh(L"Contents/suzanne.fbx"));
    MeshComponent->SetupAttachment(RootComponent);
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
