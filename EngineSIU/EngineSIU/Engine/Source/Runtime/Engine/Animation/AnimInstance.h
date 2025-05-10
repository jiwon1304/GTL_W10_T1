#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class UAnimSequenceBase;

/*
 * 일반적인 애니메이션 블루프린트 기반의 인스턴스
 * 여러 개의 애니메이션을 소유하여 블렌드 및 상태 전이 가능 - 플레이어 캐릭터 등에 사용
 */
class UAnimInstance : public UObject
{
    DECLARE_CLASS(UAnimInstance, UObject)
public:
    UAnimInstance() = default;
    void TriggerAnimNotifies(float DeltaSeconds);

    UAnimSequenceBase* Sequence = nullptr; // 본래 FAnimNode_SequencePlayer에서 소유
};
