#pragma once
#include "HAL/PlatformType.h"

struct FFrameTime
{
    uint32 FrameNumber;
    float SubFrame;
};
struct FFrameRate
{
    /**
     * Default construction to a frame rate of 60000 frames per second (0.0166 ms)
     */
    FFrameRate()
        : Numerator(60000), Denominator(1)
    {
    }

    FFrameRate(uint32 InNumerator, uint32 InDenominator)
        : Numerator(InNumerator), Denominator(InDenominator)
    {
    }

    int32 Numerator;
    int32 Denominator;

    bool IsValid() const
    {
        return Denominator > 0;
    }

    double AsInterval() const;

    double AsDecimal() const;
    double AsSeconds(FFrameTime FrameNumber) const;
    FFrameTime AsFrameTime(double InTimeSeconds) const;

    // uint32 AsFrameNumber(double InTimeSeconds) const;
    // bool IsMultipleOf(FFrameRate Other) const;
    // bool IsFactorOf(FFrameRate Other) const;
    // static FFrameTime TransformTime(FFrameTime SourceTime, FFrameRate SourceRate, FFrameRate DestinationRate);

    FFrameRate Reciprocal() const
    {
        return FFrameRate(Denominator, Numerator);
    }

    friend inline bool operator==(const FFrameRate& A, const FFrameRate& B)
    {
        return A.Numerator == B.Numerator && A.Denominator == B.Denominator;
    }

    friend inline bool operator!=(const FFrameRate& A, const FFrameRate& B)
    {
        return A.Numerator != B.Numerator || A.Denominator != B.Denominator;
    }

    friend inline FFrameRate operator*(FFrameRate A, FFrameRate B)
    {
        return FFrameRate(A.Numerator * B.Numerator, A.Denominator * B.Denominator);
    }

    friend inline FFrameRate operator/(FFrameRate A, FFrameRate B)
    {
        return FFrameRate(A.Numerator * B.Denominator, A.Denominator * B.Numerator);
    }

    //friend inline double operator/(FFrameNumber Frame, FFrameRate Rate)
    //{
    //    return Rate.AsSeconds(FFrameTime(Frame));
    //}
};

inline double FFrameRate::AsInterval() const
{
    return double(Denominator) / double(Numerator);
}

inline double FFrameRate::AsDecimal() const
{
    return double(Numerator) / double(Denominator);
}
