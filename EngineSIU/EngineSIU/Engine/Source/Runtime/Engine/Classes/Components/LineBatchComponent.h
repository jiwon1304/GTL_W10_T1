#pragma once
#include "PrimitiveComponent.h"
class ULineBatchComponent :
    public UPrimitiveComponent
{
    DECLARE_CLASS(ULineBatchComponent, UPrimitiveComponent)

public:
    ULineBatchComponent() = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    virtual void InitializeComponent() override;
    virtual void TickComponent(float DeltaTime) override;

    virtual bool IntersectRayTriangle(
        const FVector& RayOrigin, const FVector& RayDirection,
        const FVector& v0, const FVector& v1, const FVector& v2, float& OutHitDistance
    ) {
        return false;
    }

    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override {} // 저장하지 않음
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override {}

    void DrawLine(const FVector& Start, const FVector& End, const FLinearColor& Color);
    void DrawSphere(const FVector& Center, float Radius, const FLinearColor& Color, int32 Segments = 12);
    void DrawBox(const FVector& Center, const FRotator& Rotation, const FVector& Extent, const FLinearColor& Color);
    void DrawCone(const FVector& Apex, const FVector& Direction, float Radius, float Angle, const FLinearColor& Color);

private:
    /** Buffer of lines to draw */
    TArray<FBatchedLine> BatchedLines;
    /** Buffer or points to draw */
    TArray<FBatchedPoint> BatchedPoints;
    /** Default time that lines/points will draw for */
    float DefaultLifeTime;
};

