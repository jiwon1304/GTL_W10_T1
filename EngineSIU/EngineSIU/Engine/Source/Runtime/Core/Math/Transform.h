#pragma once
#include "MathUtility.h"
#include "Rotator.h"
#include "Vector.h"

/**
 * 위치, 회전, 스케일 정보를 포함하는 3D 변환(Transform) 구조체입니다.
 *
 * 이 구조체는 3D 공간에서의 변환을 표현하는 데 사용되며, 다음 요소들을 포함합니다:
 * - 위치(Translation): 3D 공간에서 객체의 이동
 * - 회전(Rotation): 객체의 방향(보통 오일러 각도로 정의)
 * - 스케일(Scale): X, Y, Z 축을 따라 객체의 크기 조정
 *
 * FTransform 클래스는 변환을 항등 상태(Identity)로 초기화하거나, 주어진 행렬로부터 초기화할 수 있는 생성자를 제공합니다.
 */
struct FTransform
{
public:
    FTransform();
    FTransform(const FMatrix& InMatrix);

public:
    FVector GetTranslation() const { return Translation; }
    void SetTranslation(const FVector& InTranslation) { Translation = InTranslation; }

    FRotator GetRotation() const { return Rotation; }
    void SetRotation(const FRotator& InRotation) { Rotation = InRotation; }

    FVector GetScale3D() const { return Scale3D; }
    void SetScale3D(const FVector& InScale3D) { Scale3D = InScale3D; }

public:
    FVector Translation;
    FRotator Rotation;
    FVector Scale3D;

public:
    friend FArchive& operator<<(FArchive& Ar, FTransform& Data)
    {
        return Ar << Data.Translation
                  << Data.Rotation
                  << Data.Scale3D;
    }
};
