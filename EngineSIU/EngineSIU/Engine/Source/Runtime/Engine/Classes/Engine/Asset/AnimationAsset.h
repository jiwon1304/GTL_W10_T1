#pragma once
#include <Animation/AnimSequenceBase.h>
#include "Engine/FbxManager.h"
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

    FString ToString() const
    {
        return FString::Printf(TEXT("Sequence=%s, CurrentTime=%f, RateScale=%f"),
            *Sequence->GetSeqName(), CurrentTime, RateScale);
    }

    void InitFromString(const FString& InString)
    {
        // Sequence 파싱: "Sequence=XXXX"에서 XXXX만 추출
        FString SequenceName;
        {
            FString SearchKey = TEXT("Sequence=");
            int32 StartIdx = InString.Find(SearchKey, ESearchCase::IgnoreCase, ESearchDir::FromStart);
            if (StartIdx != INDEX_NONE)
            {
                StartIdx += SearchKey.Len();
                int32 EndIdx = InString.Find(TEXT(","), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIdx);
                if (EndIdx == INDEX_NONE)
                {
                    SequenceName = InString.Mid(StartIdx);
                }
                else
                {
                    SequenceName = InString.Mid(StartIdx, EndIdx - StartIdx);
                }
            }
        }
        Sequence = FFbxManager::GetAnimSequenceByName(SequenceName);

        // CurrentTime 파싱 (float)
        FParse::Value(*InString, TEXT("CurrentTime="), CurrentTime);

        // RateScale 파싱 (float)
        FParse::Value(*InString, TEXT("RateScale="), RateScale);
    }
};
