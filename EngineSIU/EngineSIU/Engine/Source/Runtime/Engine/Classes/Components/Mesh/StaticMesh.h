#pragma once
#include "BaseMesh.h"
#include "UObject/ObjectMacros.h"


class UStaticMeshTest : public UMeshAsset
{
    DECLARE_CLASS(UStaticMeshTest, UMeshAsset)

public:
    UStaticMeshTest() = default;

public:
    TArray<FVertex> Vertices;
    TArray<uint32> Indices; // 32비트 인덱스 버퍼 사용 가정
    TArray<FMeshSubset> Subsets; // 머티리얼별 드로우 콜 정보
};
