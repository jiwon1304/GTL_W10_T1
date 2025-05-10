#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

struct FTransform;
class USkeletalMeshComponent;
class UAnimSequenceBase;

/*
 * 일반적인 애니메이션 블루프린트 기반의 인스턴스
 * 여러 개의 애니메이션을 소유하여 블렌드 및 상태 전이 가능 - 플레이어 캐릭터 등에 사용
 */
class UAnimInstance : public UObject
{
    DECLARE_CLASS(UAnimInstance, UObject)
public:
    UAnimInstance();
    void Initialize(USkeletalMeshComponent* InOwningComponent);

    // 매 틱마다 애니메이션을 업데이트하고 최종 포즈를 OutPose에 반환합니다.
    virtual void UpdateAnimation(float DeltaSeconds, TArray<FTransform>& OutPose);

    void TriggerAnimNotifies(float DeltaSeconds);

    void SetPlaying(bool bIsPlaying){ bPlaying = bIsPlaying;}
    bool IsPlaying() const { return bPlaying; }

    float GetCurrentTime() const { return CurrentTime; }

protected:
    UAnimSequenceBase* Sequence = nullptr; // 본래 FAnimNode_SequencePlayer에서 소유

    USkeletalMeshComponent* OwningComp; 

    float CurrentTime;
    bool bPlaying;
};
