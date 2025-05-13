#include "AnimNotifyState_SlowMotion.h"

#include "Components/SkeletalMeshComponent.h"

void UAnimNotifyState_SlowMotion::NotifyBegin(USkeletalMeshComponent* MeshComp, float Duration)
{
    if (!MeshComp || !MeshComp->GetSkeletalMesh() || !MeshComp->GetSingleNodeInstance()
        || !MeshComp->GetSingleNodeInstance()->GetCurrentSequence())
    {
        return;
    }

    UE_LOG(ELogLevel::Display, TEXT("[SlowMotion] NotifyBegin - Time Dilation ↓"));

    UAnimSequenceBase* CurrentSequence = MeshComp->GetSingleNodeInstance()->GetCurrentSequence();
    OriginalTimeDilation = CurrentSequence->GetRateScale();

    // 슬로우 모션 시작
    CurrentSequence->SetRateScale(SlowRate);
}

void UAnimNotifyState_SlowMotion::NotifyTick(USkeletalMeshComponent* MeshComp, float DeltaTime)
{
    // Tick 처리 안해도 되지만 유지할 수 있음
    UE_LOG(ELogLevel::Display,TEXT("[SlowMotion] NotifyTick - still active : %.1f"), DeltaTime);
}

void UAnimNotifyState_SlowMotion::NotifyEnd(USkeletalMeshComponent* MeshComp)
{
    if (!MeshComp || !MeshComp->GetSkeletalMesh() || !MeshComp->GetSingleNodeInstance()
        || !MeshComp->GetSingleNodeInstance()->GetCurrentSequence())
    {
        return;
    }
    UE_LOG(ELogLevel::Display, TEXT("[SlowMotion] NotifyEnd - Time Dilation ↑"));

    // 원래 속도로 복원
    UAnimSequenceBase* CurrentSequence = MeshComp->GetSingleNodeInstance()->GetCurrentSequence();
    CurrentSequence->SetRateScale(OriginalTimeDilation);
}
