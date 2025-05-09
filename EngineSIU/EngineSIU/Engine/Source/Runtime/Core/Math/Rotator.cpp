#include "Rotator.h"

#include "Vector.h"
#include "Quat.h"
#include "Matrix.h"
#include "Misc/Parse.h"

const FRotator FRotator::ZeroRotator(0.0f, 0.0f, 0.0f);

FRotator::FRotator(const FVector& InVector)
    : Pitch(FMath::RadiansToDegrees(InVector.Y)), Yaw(FMath::RadiansToDegrees(InVector.Z)), Roll(FMath::RadiansToDegrees(InVector.X))
{
}

FRotator::FRotator(const FQuat& InQuat)
{
    *this = InQuat.Rotator();
}

FRotator FRotator::operator+(const FRotator& Other) const
{
    return FRotator(Pitch + Other.Pitch, Yaw + Other.Yaw, Roll + Other.Roll);
}

FRotator& FRotator::operator+=(const FRotator& Other)
{
    Pitch += Other.Pitch; Yaw += Other.Yaw; Roll += Other.Roll;
    return *this;
}

FRotator FRotator::operator-(const FRotator& Other) const
{
    return FRotator(Pitch - Other.Pitch, Yaw - Other.Yaw, Roll - Other.Roll);
}

FRotator& FRotator::operator-=(const FRotator& Other)
{
    Pitch -= Other.Pitch; Yaw -= Other.Yaw; Roll -= Other.Roll;
    return *this;
}

FRotator FRotator::operator*(float Scalar) const
{
    return FRotator(Pitch * Scalar, Yaw * Scalar, Roll * Scalar);
}

FRotator FRotator::operator*(const FRotator& Other) const
{
    FQuat A = Quaternion();
    FQuat B = Other.Quaternion();
    FQuat Combined = A * B;
    return FromQuaternion(Combined);
}

FRotator& FRotator::operator*=(float Scalar)
{
    Pitch *= Scalar; Yaw *= Scalar; Roll *= Scalar;
    return *this;
}

FRotator FRotator::operator/(const FRotator& Other) const
{
    return FRotator(Pitch / Other.Pitch, Yaw / Other.Yaw, Roll / Other.Roll);
}

FRotator FRotator::operator/(float Scalar) const
{
    return FRotator(Pitch / Scalar, Yaw / Scalar, Roll / Scalar);
}

FRotator& FRotator::operator/=(float Scalar)
{
    Pitch /= Scalar; Yaw /= Scalar; Roll /= Scalar;
    return *this;
}

FRotator FRotator::operator-() const
{
    return FRotator(-Pitch, -Yaw, -Roll);
}

bool FRotator::operator==(const FRotator& Other) const
{
    return Pitch == Other.Pitch && Yaw == Other.Yaw && Roll == Other.Roll;
}

bool FRotator::operator!=(const FRotator& Other) const
{
    return Pitch != Other.Pitch || Yaw != Other.Yaw || Roll != Other.Roll;
}

bool FRotator::IsNearlyZero(float Tolerance) const
{
    return FMath::Abs(Pitch) <= Tolerance && FMath::Abs(Yaw) <= Tolerance && FMath::Abs(Roll) <= Tolerance;
}

bool FRotator::IsZero() const
{
    return Pitch == 0.0f && Yaw == 0.0f && Roll == 0.0f;
}

bool FRotator::Equals(const FRotator& Other, float Tolerance) const
{
    return FMath::Abs(Pitch - Other.Pitch) <= Tolerance && FMath::Abs(Yaw - Other.Yaw) <= Tolerance && FMath::Abs(Roll - Other.Roll) <= Tolerance;

}

FRotator FRotator::Add(float DeltaPitch, float DeltaYaw, float DeltaRoll) const
{
    return FRotator(Pitch + DeltaPitch, Yaw + DeltaYaw, Roll + DeltaRoll);
}

FRotator FRotator::FromQuaternion(const FQuat& InQuat) const
{
    return FRotator(InQuat);
}

FQuat FRotator::Quaternion() const
{
    return FQuat(*this);
}

FVector FRotator::ToVector() const
{
    const float PitchNoWinding = FMath::Fmod(Pitch, 360.f);
    const float YawNoWinding = FMath::Fmod(Yaw, 360.f);

    float CP, SP, CY, SY;
    FMath::SinCos( &SP, &CP, FMath::DegreesToRadians(PitchNoWinding) );
    FMath::SinCos( &SY, &CY, FMath::DegreesToRadians(YawNoWinding) );
    FVector V = FVector( CP*CY, CP*SY, SP );

    if (!_finite(V.X) || !_finite(V.Y) || !_finite(V.Z))
    {
        V = FVector::ForwardVector;
    }
    
    return V;
}

FVector FRotator::RotateVector(const FVector& Vec) const
{
    return Quaternion().RotateVector(Vec);
}

FMatrix FRotator::ToMatrix() const
{
    return FMatrix::GetRotationMatrix(*this);
}

FRotator FRotator::MakeLookAtRotation(const FVector& From, const FVector& To)
{
    FVector Dir = To - From;
    float Yaw = std::atan2(Dir.Y, Dir.X) * 180.0f / PI;
    float DistanceXY = std::sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y);
    float Pitch = std::atan2(Dir.Z, DistanceXY) * 180.0f / PI;
    float Roll = 0.0f;
    return FRotator(Pitch, Yaw, Roll);
}

FRotator FRotator::GetInverse() const
{
    // 1) 현재 Rotator → 쿼터니언  
    const FQuat Q = Quaternion();

    // 2) 쿼터니언 역연산  
    const FQuat InvQ = Q.GetInverse();

    // 3) 역쿼터니언 → Rotator  
    FRotator InvRot = InvQ.Rotator();

    // 4) 각도 범위 정리(Optional)  
    InvRot.Normalize();

    return InvRot;
}

float FRotator::ClampAxis(float Angle)
{
    Angle = FMath::Fmod(Angle, 360.0f);
    if (Angle < 0.0f)
    {
        Angle += 360.0f;
    }
    return Angle;
}

FRotator FRotator::GetNormalized() const
{
    return FRotator(FMath::UnwindDegrees(Pitch), FMath::UnwindDegrees(Yaw), FMath::UnwindDegrees(Roll));
}

void FRotator::Normalize()
{
    Pitch = FMath::UnwindDegrees(Pitch);
    Yaw = FMath::UnwindDegrees(Yaw);
    Roll = FMath::UnwindDegrees(Roll);
}

FString FRotator::ToString() const
{
    return FString::Printf(TEXT("Pitch=%3.3f Yaw=%3.3f Roll=%3.3f"), Pitch, Yaw, Roll);
}

bool FRotator::InitFromString(const FString& InSourceString)
{
    Pitch = 0.0f;
    Yaw = 0.0f;
    Roll = 0.0f;

    const bool bSuccess = FParse::Value(*InSourceString, TEXT("Pitch="), Pitch) &&
        FParse::Value(*InSourceString, TEXT("Yaw="), Yaw) &&
        FParse::Value(*InSourceString, TEXT("Roll="), Roll);

    return bSuccess;
}

float FRotator::NormalizeAxis(float Angle)
{
    Angle = ClampAxis(Angle);

    if (Angle > 180.0f)
    {
        // shift to (-180,180]
        Angle -= 360.0f;
    }

    return Angle;
}
