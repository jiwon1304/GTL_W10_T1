#include "PreviewAnimInstance.h"

#include "Animation/AnimationStateMachine.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectFactory.h"

void UPreviewAnimInstance::NativeInitializeAnimation()
{
    UAnimInstance::NativeInitializeAnimation();

    UAnimNode_State* Idle = FObjectFactory::ConstructObject<UAnimNode_State>(this);
    Idle->Initialize(FName("Idle"), Anim1);
    Idle->GetLinkAnimationSequenceFunc = [this]() { return Anim1; };
    AnimSM->AddState(Idle);

    UAnimNode_State* Walk = FObjectFactory::ConstructObject<UAnimNode_State>(this);
    Walk->Initialize(FName("Walk"), Anim2);
    Walk->GetLinkAnimationSequenceFunc = [this]() { return Anim2; };
    AnimSM->AddState(Walk);

    UAnimNode_State* Jump = FObjectFactory::ConstructObject<UAnimNode_State>(this);
    Jump->Initialize(FName("Jump"), Anim3);
    Jump->GetLinkAnimationSequenceFunc = [this]() { return Anim3; };
    AnimSM->AddState(Jump);

    AnimSM->AddTransition(Idle, Walk, [this]() { return this->bTransition[0]; });
    AnimSM->AddTransition(Walk, Idle, [this]() { return this->bTransition[1]; });
    
    AnimSM->AddTransition(Walk, Jump, [this]() { return this->bTransition[2]; });
    AnimSM->AddTransition(Jump, Walk, [this]() { return this->bTransition[3]; });

    AnimSM->AddTransition(Idle, Jump, [this]() { return this->bTransition[4]; });
    AnimSM->AddTransition(Jump, Idle, [this]() { return this->bTransition[5]; });

    AnimSM->SetState(FName("Idle"));
}

void UPreviewAnimInstance::UpdateAnimation(float DeltaSeconds)
{
    Super::UpdateAnimation(DeltaSeconds);
}
