#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class USkeletalMeshComponent;

/* [구버전 기준으로 작성]
*  애니메이션의 Duration, SkeletalMesh만 전달
*  Notify가 어디서 왔는지 (Montage or Sequence), 어떤 이름인지 등 정보 부족
*/
class UAnimNotifyState : public UObject
{
    DECLARE_ABSTRACT_CLASS(UAnimNotifyState, UObject);
public:
    UAnimNotifyState() = default;

    // Notify가 시작될 때 호출 (TriggerTime 시점)
    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, float Duration) = 0;

    // Notify가 활성화된 동안 매 프레임 호출
    virtual void NotifyTick(USkeletalMeshComponent* MeshComp, float DeltaTime) = 0;

    // Notify가 끝날 때 호출 (EndTriggerTime 시점)
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp) = 0;

    virtual void GetProperties(TMap<FString, FString>& OutProperties) const = 0;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) = 0;
    int a;
};
