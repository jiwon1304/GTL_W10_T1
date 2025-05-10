#pragma once
#include "AnimSequenceBase.h"

/*
 * 전체 시퀀스 : FBX의 FbxAnimStack에 대응
 */
class UAnimSequence : public UAnimSequenceBase
{
    DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)

public:
    UAnimSequence() = default;
    //virtual UObject* Duplicate(UObject* InOuter) override;


};
