#include "LuaAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void ULuaAnimInstance::UpdateAnimation(float DeltaSeconds)
{
    UAnimInstance::UpdateAnimation(DeltaSeconds);
    
    bIsWalking = OwningComp->GetOwner()->GetActorLocation().X > 10.f;
}

void ULuaAnimInstance::SetLuaFunction()
{
    UAnimInstance::SetLuaFunction();

    LuaState.set_function("IsWalking", [this]() { return this->bIsWalking; });
}
