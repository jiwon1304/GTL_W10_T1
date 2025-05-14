#pragma once
#include <Animation/AnimSequenceBase.h>
// 하나의 애니메이션 클립에 대한 시간 진행 상태와 재생 속도 정보를 캡슐화.

struct FBlendState
{
    UAnimSequenceBase* Sequence = nullptr;      // 애니메이션 시퀀스 포인터
    float CurrentTime = 0.0f;       // 애니메이션 내에서의 재생 시간 (초 단위)
    float RateScale = 1.0f;         // 재생 속도 배율

    // 외부 Tick 시간에 따라 CurrentTime 자동 증가.
    void Advance(float DeltaSeconds) {
        CurrentTime += DeltaSeconds * RateScale;
    }

    // 유효한 애니메이션 상태 확인.
    bool IsValid() const { return Sequence != nullptr; }
};
