#pragma once
#include "AnimSequenceBase.h"

class UAnimSequence : public UAnimSequenceBase
{
    DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)
public:
    UAnimSequence() = default;
    virtual UObject* Duplicate(UObject* InOuter) override;


};
