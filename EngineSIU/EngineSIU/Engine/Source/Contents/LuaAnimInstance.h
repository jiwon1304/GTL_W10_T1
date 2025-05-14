#pragma once
#include "Animation/AnimInstance.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"


class ULuaAnimInstance : public UAnimInstance
{
    DECLARE_CLASS(ULuaAnimInstance, UAnimInstance)

public:
    ULuaAnimInstance() = default;

    virtual void UpdateAnimation(float DeltaSeconds) override;
    virtual void SetLuaFunction() override;

private:
    bool bIsWalking = false;
};
