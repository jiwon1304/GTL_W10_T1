#include "MyAnimInstance.h"

#include "Animation/AnimationStateMachine.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectFactory.h"

void UMyAnimInstance::NativeInitializeAnimation()
{
    UAnimInstance::NativeInitializeAnimation();

    UAnimNode_State* Idle = FObjectFactory::ConstructObject<UAnimNode_State>(this);
    Idle->Initialize(FName("Idle"), Anim1);
    Idle->GetLinkAnimationSequenceFunc = [this]() { return Anim1; };

    UAnimNode_State* Fly = FObjectFactory::ConstructObject<UAnimNode_State>(this);
    Fly->Initialize(FName("Fly"), Anim2);
    Fly->GetLinkAnimationSequenceFunc = [this]() { return Anim2; };
    
    AnimSM->AddTransition(Idle, Fly, [this]() { return this->bIsFly; });
    AnimSM->AddTransition(Fly, Idle, [this]() { return !this->bIsFly; });

    AnimSM->SetState(FName("Idle"));
}

void UMyAnimInstance::UpdateAnimation(float DeltaSeconds)
{
    Super::UpdateAnimation(DeltaSeconds);


    if (OwningComp->GetOwner()->GetActorLocation().Z > 10.f)
    {
        bIsFly = true;
    }
    else
    {
        bIsFly = false;
    }
}
