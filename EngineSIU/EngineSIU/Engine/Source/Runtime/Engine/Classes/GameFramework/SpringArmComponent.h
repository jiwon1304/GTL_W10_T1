#pragma once

#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "PropertyEditor/PropertyEditorPanel.h"


class USpringArmComponent : public USceneComponent
{
    DECLARE_CLASS(USpringArmComponent, USceneComponent)
    friend class PropertyEditorPanel;
public:
    USpringArmComponent();
    virtual ~USpringArmComponent() override = default;

    virtual void TickComponent(float DeltaTime) override;

    void UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime);

    FVector BlendLocations(const FVector& DesiredArmLocation, const FVector& TraceHitLocation, bool bHitSomething, float DeltaTime);

    static bool RaySweepBox(const FVector& Start, const FVector& Dir, float MaxDist, const FMatrix& BoxMatrix,
                            const FVector& BoxExtents, float ProbeRadius, float& OutT);

public:
    FRotator GetDesiredRotation() const;
    FRotator GetTargetRotation() const;
//private:
//    FVector TargetOffset;           // 월드 공간 오프셋
//    float TargetArmLength;          // 카메라와의 거리
//
//    float ProbeSize;                // 충돌에 사용할 구 반지름
//
//    uint32 bDoCollisionTest : 1;    // 암 길이 자동으로 줄여 충돌을 방지할지 여부
//    uint32 bUsePawnControlRotation : 1; // 부모의 회전값을 사용할지 여부
//
//    uint32 bInheritPitch : 1;
//    uint32 bInheritYaw : 1;
//    uint32 bInheritRoll : 1;
//
//    uint32 bEnableCameraLag : 1;         // 위치 지연 여부
//    uint32 bEnableCameraRotationLag : 1; // 회전 지연 여부
//    uint32 bUseCameraLagSubstepping : 1; // sub-stepping으로 작은 조각으로 보간
//
//    float CameraLagSpeed;                // 위치 지연 속도
//    float CameraRotationLagSpeed;        // 회전 지연 속도
//    float CameraLagMaxTimeStep;          // sub-stepping의 마지노선 
//    float CameraLagMaxDistance;          // 목표 위치로부터 지연 위치의 최대 상한
//
//
//    /* 이전 프레임의 위치 회전 등 저장할 임시 변수 */
//    FVector PreviousDesiredLoc;
//    FVector PreviousArmOrigin;
//    FRotator PreviousDesiredRot;
//
//
//    FVector RelativeSocketLocation; // 암을 따라 회전되는 로컬 오프셋
//    FQuat RelativeSocketRotation;
    // ------------------- ImGui-exposed UProperties -------------------
    UPROPERTY(EditAnywhere, FVector, TargetOffset, = FVector::ZeroVector)
    UPROPERTY(EditAnywhere, float, TargetArmLength, = 300.0f)
    UPROPERTY(EditAnywhere, float, ProbeSize, = 12.0f)

    UPROPERTY(EditAnywhere, bool, bDoCollisionTest, = true)
    UPROPERTY(EditAnywhere, bool, bUsePawnControlRotation, = false)

    UPROPERTY(EditAnywhere, bool, bInheritPitch, = true)
    UPROPERTY(EditAnywhere, bool, bInheritYaw, = true)
    UPROPERTY(EditAnywhere, bool, bInheritRoll, = true)

    UPROPERTY(EditAnywhere, bool, bEnableCameraLag, = false)
    UPROPERTY(EditAnywhere, bool, bEnableCameraRotationLag, = false)
    UPROPERTY(EditAnywhere, bool, bUseCameraLagSubstepping, = false)

    UPROPERTY(EditAnywhere, float, CameraLagSpeed, = 10.0f)
    UPROPERTY(EditAnywhere, float, CameraRotationLagSpeed, = 10.0f)
    UPROPERTY(EditAnywhere, float, CameraLagMaxTimeStep, = 0.05f)
    UPROPERTY(EditAnywhere, float, CameraLagMaxDistance, = 100.0f)

private:
    /* 이전 프레임의 위치 회전 등 저장할 임시 변수 */
    FVector PreviousDesiredLoc;
    FVector PreviousArmOrigin;
    FRotator PreviousDesiredRot;

    FVector RelativeSocketLocation; // 암을 따라 회전되는 로컬 오프셋
    FQuat RelativeSocketRotation;
};
