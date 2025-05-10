#pragma once
#include "Container/Array.h"
#include "Math/Color.h"
#include "UObject/NameTypes.h"

struct FVectorCurve
{
};


struct FFloatCurve
{
    FName CurveName;
    FLinearColor Color;
};

struct FTransformCurve
{
    FVectorCurve	TranslationCurve;
    FVectorCurve	RotationCurve;
    FVectorCurve	ScaleCurve;
};

struct FAnimationCurveData
{
    TArray<FFloatCurve> FloatCurves;
    TArray<FTransformCurve> TransformCurves;
};
