#include "PreviewAnimInstance.h"

#include "Animation/AnimationStateMachine.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectFactory.h"

void UPreviewAnimInstance::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);
    const FString* TempStr = nullptr;
    TempStr = InProperties.Find(TEXT("UPreviewAnimInstance::Anim1"));
    if (TempStr)
    {
        if (UAnimSequenceBase* AnimSeq = FFbxManager::GetAnimSequenceByName(*TempStr))
        {
            Anim1 = AnimSeq;
        }
        else
        {
            UE_LOG(ELogLevel::Warning, TEXT("SetProperties: AnimSequence '%s' not found"), **TempStr);
            Anim1 = nullptr;
        }
    }
    TempStr = InProperties.Find(TEXT("UPreviewAnimInstance::Anim2"));
    if (TempStr)
    {
        if (UAnimSequenceBase* AnimSeq = FFbxManager::GetAnimSequenceByName(*TempStr))
        {
            Anim2 = AnimSeq;
        }
        else
        {
            UE_LOG(ELogLevel::Warning, TEXT("SetProperties: AnimSequence '%s' not found"), **TempStr);
            Anim2 = nullptr;
        }
    }
    TempStr = InProperties.Find(TEXT("UPreviewAnimInstance::Anim3"));
    if (TempStr)
    {
        if (UAnimSequenceBase* AnimSeq = FFbxManager::GetAnimSequenceByName(*TempStr))
        {
            Anim3 = AnimSeq;
        }
        else
        {
            UE_LOG(ELogLevel::Warning, TEXT("SetProperties: AnimSequence '%s' not found"), **TempStr);
            Anim3 = nullptr;
        }
    }

    TempStr = InProperties.Find(TEXT("UPreviewAnimInstance::bTransition"));
    if (TempStr)
    {
        const TCHAR* Ptr = **TempStr;
        int32 i = 0;
        while (*Ptr && i < 6)
        {
            // 값 추출
            FString NumStr;
            while (*Ptr && *Ptr != TEXT(','))
            {
                NumStr = NumStr + FString(Ptr);
                ++Ptr;
            }
            // 문자열이 비어있지 않다면 변환
            if (!NumStr.IsEmpty())
            {
                bTransition[i] = (FCString::Atoi(*NumStr) != 0);
                ++i;
            }
            // 콤마 건너뛰기
            if (*Ptr == TEXT(',')) ++Ptr;
        }
        // 남은 값이 부족하면 false로 초기화
        for (; i < 6; ++i)
        {
            bTransition[i] = false;
        }
    }
}

void UPreviewAnimInstance::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
    OutProperties.Add(TEXT("UPreviewAnimInstance::Anim1"), Anim1->GetSeqName());
    OutProperties.Add(TEXT("UPreviewAnimInstance::Anim2"), Anim2->GetSeqName());
    OutProperties.Add(TEXT("UPreviewAnimInstance::Anim3"), Anim3->GetSeqName());
    OutProperties.Add(TEXT("UPreviewAnimInstance::bTransition"), FString::Printf(TEXT("%d,%d,%d,%d,%d,%d"),
        bTransition[0], bTransition[1], bTransition[2], bTransition[3], bTransition[4], bTransition[5]));
}

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
