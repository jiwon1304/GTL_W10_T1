#include "SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"


ASkeletalMeshActor::ASkeletalMeshActor()
{
    SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
}
