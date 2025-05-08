#include "Transform.h"
#include "Matrix.h"


FTransform::FTransform()
    : Translation(FVector::ZeroVector)
    , Rotation(FRotator::ZeroRotator)
    , Scale3D(FVector::OneVector)
{
}

FTransform::FTransform(const FMatrix& InMatrix)
{
    // Extract translation
    Translation = InMatrix.GetTranslationVector();

    // Extract rotation and scale from the matrix
    FMatrix RotationMatrix = InMatrix;
    Scale3D = RotationMatrix.ExtractScaling();

    // If there is negative scaling, handle it
    if (InMatrix.Determinant() < 0.0f)
    {
        // Reflect matrix to ensure proper handedness
        Scale3D *= -1.0f;
        RotationMatrix.SetAxis(0, -RotationMatrix.GetScaledAxis(0));
    }

    Rotation = RotationMatrix.Rotator();
}
