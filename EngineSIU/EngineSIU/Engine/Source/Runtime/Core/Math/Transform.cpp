
#include "Transform.h"

#include "JungleMath.h"
#include "Misc/Parse.h"

const FTransform FTransform::Identity = FTransform(FVector::ZeroVector, FQuat::Identity, FVector::OneVector);

FTransform::FTransform(const FMatrix& InMatrix)
{
    // Extract translation
    Translation = InMatrix.GetTranslationVector();

    // Extract rotation and scale from the matrix
    FMatrix RotationMatrix = InMatrix;
    Scale3D = RotationMatrix.ExtractScaling(SMALL_NUMBER);

    // If there is negative scaling, handle it
    if (InMatrix.Determinant() < 0.0f)
    {
        // Reflect matrix to ensure proper handedness
        Scale3D *= -1.0f;
        RotationMatrix.SetAxis(0, -RotationMatrix.GetScaledAxis(0));
    }

    Rotation = RotationMatrix.ToQuat();
}

FMatrix FTransform::GetViewMatrix() const
{
    return JungleMath::CreateViewMatrix(Translation, Translation + GetForward(), FVector::UpVector);
}

FMatrix FTransform::GetScaleMatrix() const
{
    return FMatrix::GetScaleMatrix(Scale3D);
}

FMatrix FTransform::GetMatrix() const
{
    return FMatrix::GetScaleMatrix(Scale3D)
        * FMatrix::GetRotationMatrix(Rotation)
        * FMatrix::GetTranslationMatrix(Translation);
}

FMatrix FTransform::GetLocalMatrixWithOutScale() const
{
    return FMatrix::GetRotationMatrix(Rotation)
        * FMatrix::GetTranslationMatrix(Translation);
}

FMatrix FTransform::ToMatrixWithScale() const
{
    // Scale, Rotation, Translation 순서로 행렬을 생성
    return FMatrix::GetScaleMatrix(Scale3D)
        * FMatrix::GetRotationMatrix(Rotation)
        * FMatrix::GetTranslationMatrix(Translation);
}

FVector FTransform::GetForward() const
{
    const FMatrix RotationMatrix = FMatrix::GetRotationMatrix(Rotation);
    const FVector Forward = FVector(
        RotationMatrix.M[0][0],
        RotationMatrix.M[1][0],
        RotationMatrix.M[2][0]
    );
    return Forward.GetSafeNormal();
}

FVector FTransform::GetRight() const
{
    const FMatrix RotationMatrix = FMatrix::GetRotationMatrix(Rotation);
    const FVector Right = FVector(
        RotationMatrix.M[0][1],
        RotationMatrix.M[1][1],
        RotationMatrix.M[2][1]
    );
    return Right.GetSafeNormal();
}

FVector FTransform::GetUp() const
{
    const FMatrix RotationMatrix = FMatrix::GetRotationMatrix(Rotation);
    const FVector up = FVector(
        RotationMatrix.M[0][2],
        RotationMatrix.M[1][2],
        RotationMatrix.M[2][2]
    );
    return up.GetSafeNormal();
}

void FTransform::Translate(const FVector& InTranslation)
{
    Translation += InTranslation;
}

void FTransform::Rotate(const FRotator& InRotation)
{
    RotateRoll(InRotation.Roll);
    RotatePitch(InRotation.Pitch);
    RotateYaw(InRotation.Yaw);
}

void FTransform::RotateYaw(const float AngleDegrees)
{
    // Z축(Up) 기준 Yaw 회전
    const float HalfRad = FMath::DegreesToRadians(AngleDegrees) * 0.5f;
    const float SinH = FMath::Sin(HalfRad);
    const float CosH = FMath::Cos(HalfRad);
    FQuat DeltaQuat(FVector(0, 0, 1) * SinH, CosH);
    DeltaQuat.Normalize();

    Rotation = ((DeltaQuat)*Rotation);
}

void FTransform::RotatePitch(const float AngleDegrees)
{
    // Y축(Right) 기준 Pitch 회전
    const float HalfRad = FMath::DegreesToRadians(AngleDegrees) * 0.5f;
    const float SinH = FMath::Sin(HalfRad);
    const float CosH = FMath::Cos(HalfRad);
    FQuat DeltaQuat(FVector(0, 1, 0) * SinH, CosH);
    DeltaQuat.Normalize();

    Rotation = (DeltaQuat)*Rotation;
}

void FTransform::RotateRoll(const float AngleDegrees)
{
    // X축(Forward) 기준 Roll 회전
    const float HalfRad = FMath::DegreesToRadians(AngleDegrees) * 0.5f;
    const float SinH = FMath::Sin(HalfRad);
    const float CosH = FMath::Cos(HalfRad);
    FQuat DeltaQuat(FVector(1, 0, 0) * SinH, CosH);
    DeltaQuat.Normalize();

    Rotation = (DeltaQuat)*Rotation;
}


void FTransform::MoveLocal(const FVector& InTranslation)
{
    const FMatrix transfromMatrix = GetMatrix();
    const FVector worldDelta = FMatrix::TransformVector(InTranslation, transfromMatrix);
    Translation += worldDelta;
}

void FTransform::SetFromMatrix(const FMatrix& InMatrix)
{
    FMatrix matrix = InMatrix;

    // 행렬에서 스케일 추출
    FVector scale = matrix.ExtractScaling();
    Scale3D = scale;

    // 행렬식이 음수면 스케일 부호를 보정
    if (matrix.Determinant() < 0)
    {
        scale *= -1;
        Scale3D = Scale3D * FVector(-1.f, 1.f, 1.f);
    }

    // 회전 부분을 쿼터니언으로 변환
    const FQuat quat = FQuat(matrix);
    Rotation = quat;

    // Translation(이동) 추출
    const FVector translation = matrix.GetTranslationVector();
    Translation = translation;
}

FVector4 FTransform::TransformVector4(const FVector4& Vector) const
{
    // 입력 4D 벡터에서 x,y,z를 추출
    const FVector vec3 = FVector(Vector.X, Vector.Y, Vector.Z);

    // 스케일 적용
    const FVector scaledVec3 = vec3 * Scale3D;

    // 회전 적용
    const FVector rotatedVec3 = Rotation.RotateVector(scaledVec3);

    // Translation은 W 값에 따라 적용 (W가 1이면 적용, 0이면 무시)
    const FVector translatedVec3 = rotatedVec3 + (Translation * Vector.W);

    FVector4 result;
    result.X = translatedVec3.X;
    result.Y = translatedVec3.Y;
    result.Z = translatedVec3.Z;
    result.W = Vector.W;  // W값은 그대로 유지
    return result;
}

FVector FTransform::TransformPosition(const FVector& Vector) const
{
    const FVector Scaled = Vector * Scale3D;
    const FVector Rotated = Rotation.RotateVector(Scaled);
    return Rotated + Translation;
}


FVector FTransform::TransformVector(const FVector& Vector) const
{
    const FVector scaledVec3 = Vector * Scale3D;
    const FVector rotatedVec3 = Rotation.RotateVector(scaledVec3);
    return rotatedVec3;
}

FTransform FTransform::Inverse() const
{
    const FVector4 InverseScale = FVector4(1.f / Scale3D.X, 1.f / Scale3D.Y, 1.f / Scale3D.Z, 0.f);
    const FQuat InverseRotation = Rotation.GetInverse();

    const FVector ScaledTranslation = FVector(InverseScale.X * Translation.X, InverseScale.Y * Translation.Y, InverseScale.Z * Translation.Z);
    const FVector RotatedTranslation = InverseRotation.RotateVector(-ScaledTranslation);

    return FTransform(RotatedTranslation, InverseRotation, FVector(InverseScale.X, InverseScale.Y, InverseScale.Z));
}

FTransform FTransform::operator*(const FTransform& Other) const
{
    FVector ResultScale = Scale3D * Other.GetScale3D();
    FQuat ResultRotation = Rotation * Other.GetRotation();
    FVector ScaledPosition = Rotation.ToMatrix().TransformPosition(Other.GetTranslation() * Scale3D);
    FVector ResultPosition = Translation + ScaledPosition;

    return FTransform{ ResultPosition, ResultRotation, ResultScale };
}

FString FTransform::ToString() const
{
    return FString::Printf(TEXT("Translation: %s, Rotation: %s, Scale: %s"), *Translation.ToString(), *Rotation.ToString(), *Scale3D.ToString());
}

bool FTransform::InitFromString(const FString& InSourceString)
{
    // 초기화
    Translation = FVector::ZeroVector;
    Rotation = FQuat::Identity;
    Scale3D = FVector(1, 1, 1);

    // Translation 파싱
    FString TranslationString;
    if (!FString::ParseValueString(*InSourceString, TEXT("Translation:"), TranslationString))
    {
        return false;
    }

    // Rotation 파싱
    FString RotationString;
    if (!FString::ParseValueString(*InSourceString, TEXT("Rotation:"), RotationString))
    {
        return false;
    }

    // Scale 파싱
    FString ScaleString;
    if (!FString::ParseValueString(*InSourceString, TEXT("Scale:"), ScaleString))
    {
        return false;
    }

    // 각 요소별 InitFromString 호출
    bool bSuccess = true;
    bSuccess &= Translation.InitFromString(TranslationString);
    bSuccess &= Rotation.InitFromString(RotationString);
    bSuccess &= Scale3D.InitFromString(ScaleString);

}
