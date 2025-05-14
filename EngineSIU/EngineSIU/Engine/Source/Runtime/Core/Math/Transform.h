#pragma once
#include "Quat.h"
#include "Vector.h"

struct FTransform
{
public:
    FVector Translation;
    FQuat Rotation;
    FVector Scale3D;

public:
    static const FTransform Identity;

    FTransform()
        : Translation(FVector::ZeroVector)
        , Rotation(FQuat(0, 0, 0, 1))
        , Scale3D(FVector::OneVector)
    {
    }

    /*FTransform(const FVector InTranslation, const FRotator& InRotation, const FVector InScale)
        : Translation(InTranslation)
        , Scale3D(InScale)
    {
        Rotation = InRotation;
    }*/

    FTransform(const FVector InTranslation, const FQuat InQuat, const FVector InScale)
        : Translation(InTranslation)
        , Rotation(InQuat)
        , Scale3D(InScale)
    {
    }

    FTransform(const FMatrix& InMatrix);

    FORCEINLINE bool Equal(const FTransform& Other) const
    {
        return Translation == Other.Translation && Rotation.Equals(Other.Rotation)&& Scale3D == Other.Scale3D;
    }

    // 뷰 행렬을 구하는 함수 (왼손 좌표계, LookAtLH 방식)
    // 카메라의 위치(Position), 카메라가 바라보는 위치(Position + GetForward()), 그리고 카메라의 Up 벡터를 사용함.
    FMatrix GetViewMatrix() const;

    // 객체의 위치를 설정하는 함수 (벡터를 인자로 받음)
    FORCEINLINE void SetTranslation(const FVector& InTranslation)
    {
        Translation = InTranslation;
    }

    // 객체의 위치를 설정하는 함수 (x, y, z 값을 각각 인자로 받음)
    FORCEINLINE void SetTranslation(float x, float y, float z)
    {
        Translation = { x, y, z };
    }

    FORCEINLINE void SetRotation(const FQuat& InQuat)
    {
        Rotation = InQuat;
    }

    // 객체의 회전을 설정하는 함수 (x, y, z Euler 각도를 각각 인자로 받음)
    inline void SetRotation(const float x, const float y, const float z, const float w)
    {
        SetRotation(FQuat(x, y, z, w));
    }

    // 객체의 스케일을 설정하는 함수 (벡터로 설정)
    FORCEINLINE void SetScale3D(const FVector InScale)
    {
        Scale3D = InScale;
    }

    // 객체의 스케일에 값을 더하는 함수 (각 축별로 증가)
    FORCEINLINE void AddScale(const FVector InScale)
    {
        Scale3D.X += InScale.X;
        Scale3D.Y += InScale.Y;
        Scale3D.Z += InScale.Z;
    }

    // 객체의 스케일을 설정하는 함수 (x, y, z 값을 각각 인자로 받음)
    FORCEINLINE void SetScale(float x, float y, float z)
    {
        Scale3D = { x, y, z };
    }

    // 객체의 현재 위치를 반환하는 함수
    FVector GetTranslation() const
    {
        return Translation;
    }

    // 객체의 현재 회전을 반환하는 함수 (쿼터니언으로 반환)
    FQuat GetRotation() const
    {
        return Rotation;
    }

    // 객체의 현재 스케일을 반환하는 함수
    FVector GetScale3D() const
    {
        return Scale3D;
    }

    // 스케일 행렬을 반환하는 함수 (객체의 스케일 값으로 구성된 행렬)
    FMatrix GetScaleMatrix() const;

    // 전체 변환 행렬을 반환하는 함수
    // 구성 순서: Scale 행렬 * Rotation 행렬 * Translation 행렬
    FMatrix GetMatrix() const;

    // 스케일을 제외한 로컬 변환 행렬(회전 및 이동만 적용된 행렬)을 반환하는 함수
    FMatrix GetLocalMatrixWithOutScale() const;
    FMatrix ToMatrixWithScale() const;

    // 객체가 바라보는 전방 벡터를 반환하는 함수
    // 회전 행렬로 변환한 후, 회전 행렬의 첫 번째 열(Forward 벡터)을 추출
    FVector GetForward() const;

    // 객체의 오른쪽 벡터를 반환하는 함수
    FVector GetRight() const;

    // 객체의 Up 벡터를 반환하는 함수
    FVector GetUp() const;

    // 객체를 이동시키는 함수 (현재 위치에 InTranslation 값을 더함)
    void Translate(const FVector& InTranslation);

    // Euler 각도(도 단위)를 사용하여 객체를 회전시키는 함수
    // InRotation의 X, Y, Z 성분에 따라 Roll, Pitch, Yaw 회전을 차례로 적용
    void Rotate(const FRotator& InRotation);

    // Yaw 회전 (Z축 기준 회전)을 적용하는 함수
    void RotateYaw(float Angle);

    // Pitch 회전 (Y축 기준 회전)을 적용하는 함수
    void RotatePitch(float Angle);

    // Roll 회전 (X축 기준 회전)을 적용하는 함수
    void RotateRoll(float Angle);

    // 객체를 로컬 좌표계에서 이동시키는 함수
    // 로컬 변환 행렬을 사용해 InTranslation 벡터를 월드 좌표로 변환한 후 현재 위치에 더함
    void MoveLocal(const FVector& InTranslation);

    // InMatrix 행렬에서 스케일, 회전(쿼터니언), Translation을 추출하여 현재 변환을 설정하는 함수
    void SetFromMatrix(const FMatrix& InMatrix);

    // 4D 벡터를 이 변환으로 변환하는 함수.
    // 입력 벡터의 W 값이 1이면 점(위치), 0이면 방향으로 취급한다.
    // 변환 공식: V' = Q.Rotate(S * V) + T, 단 x,y,z에만 적용되고, Translation은 W에 따라 적용됨.
    FVector4 TransformVector4(const FVector4& Vector) const;

    // 점(위치) 변환 함수: P' = Q.Rotate(S * P) + T
    FVector TransformPosition(const FVector& Vector) const;

    // 벡터(방향) 변환 함수: V' = Q.Rotate(S * V)
    // Translation은 방향 벡터에 적용되지 않음.
    FVector TransformVector(const FVector& Vector) const;

    FTransform Inverse() const;

    FTransform operator*(const FTransform& Other) const;

    FORCEINLINE void NormalizeRotation()
    {
        Rotation.Normalize();
    }

    FString ToString() const;

    bool InitFromString(const FString& InSourceString);

    FORCEINLINE void Blend(const FTransform& A, const FTransform& B, float Alpha)
    {
        // 위치 선형 보간
        Translation = A.Translation * (1.0f - Alpha) + B.Translation * Alpha;
        // 스케일 선형 보간
        Scale3D = A.Scale3D * (1.0f - Alpha) + B.Scale3D * Alpha;
        // 회전 구면선형 보간(Slerp) 후 정규화
        Rotation = FQuat::Slerp(A.Rotation, B.Rotation, Alpha).GetNormalized();
    }

};
