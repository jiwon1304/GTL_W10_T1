#pragma once
#pragma once
#include "AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Classes/Engine/Asset/AnimationAsset.h"

//class UCurveFloat;

/*
*    두 개의 애니메이션을 블랜딩하여 중간 결과 포즈 생성하는 애니메이션 인스턴스
*/
class UAnimTwoNodeBlendInstance : public UAnimInstance
{
    DECLARE_CLASS(UAnimTwoNodeBlendInstance, UAnimInstance)
public:
    UAnimTwoNodeBlendInstance() = default;
    

    virtual void GetProperties(TMap<FString, FString>& OutProperties) const;
    virtual void SetProperties(const TMap<FString, FString>& InProperties);

    // bResetTimeAndState= true면 두 애니메이션 시간 0으로 초기화.
    void SetAnimationAsset(UAnimSequenceBase* NewAnimToPlay, bool bResetTimeAndState, float InBlendTime = 0.2f);
    // 매 프레임마다 호출되어 BlendAlpha 업데이트 후, 두 애니메이션의 시간을 전진.
    virtual void UpdateAnimation(float DeltaSeconds) override;

    // From과 To의 PoseA, PoseB를 계산 후 보간하여 최종 포즈 생성(CurrentPose)
    virtual const TArray<FTransform>& EvaluateAnimation() override;

    UAnimSequenceBase* GetFromSequence() const;
    UAnimSequenceBase* GetToSequence() const;

    void StopBlend(bool bResetPose = true);

    // TODO 전이 완료 시 델리게이트 호출
    //DECLARE_DELEGATE(FOnBlendComplete);
    // FOnBlendComplete OnBlendComplete;

    //void SetBlendCurve(UCurveFloat* InCurve);

protected:
    FBlendState From;               // 블렌딩 이전 관련 상태
    FBlendState To;                 // 블렌딩 이후 관련 상태

    float BlendAlpha = 0.0f;        // 0이면 From 100%, 1이면 To 100%
    float  BlendTime = 0.2f;        // 블렌딩에 걸리는 시간
    float ElaspedBlendTime = 0.0f;
    bool bIsBlending = false;
    
    // TODO : 블렌드 커브 반영
    //UCurveFloat* BlendCurve = nullptr;

    TArray<FTransform> PoseA;
    TArray<FTransform> PoseB;
};
