#pragma once
#include "Container/Array.h"
#include "Container/Map.h"
#include "Math/Color.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "UObject/NameTypes.h"


// DX11 Input Layout과 일치시킬 정점 구조체
struct FVertex
{
    FVector Position;
    FLinearColor Color;
    FVector Normal;
    FVector4 Tangent;
    FVector2D TexCoord;
    uint32 MaterialIndex;

public:
    friend FArchive& operator<<(FArchive& Ar, FVertex& Data)
    {
        return Ar << Data.Position
                  << Data.Color
                  << Data.Normal
                  << Data.Tangent
                  << Data.TexCoord
                  << Data.MaterialIndex;
    }
};

// 스켈레탈 메쉬용 정점 구조체
struct FSkinnedVertex : public FVertex
{
    // 최대 4개의 뼈 영향 가정 (DX11 Input Layout 시맨틱: BLENDINDICES, BLENDWEIGHTS)
    uint8 BoneIndices[4] = { 0, 0, 0, 0 };
    float BoneWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // 합은 1.0이 되도록 정규화

public:
    friend FArchive& operator<<(FArchive& Ar, FSkinnedVertex& Data)
    {
        Ar << static_cast<FVertex&>(Data);

        for (uint8& Idx : Data.BoneIndices)
        {
            Ar << Idx;
        }

        for (float& Weight : Data.BoneWeights)
        {
            Ar << Weight;
        }

        return Ar;
    }
};

// 단일 드로우 콜에 해당하는 메쉬의 일부 (머티리얼별로 나뉨)
struct FMeshSubset
{
    int32 MaterialIndex = -1;      // 이 서브셋에 적용될 머티리얼 인덱스
    uint32 IndexCount = 0;         // 이 서브셋을 그리는 데 사용될 인덱스 개수
    uint32 StartIndexLocation = 0; // 전체 인덱스 버퍼에서의 시작 위치
    int32 BaseVertexLocation = 0;  // 전체 버텍스 버퍼에서의 시작 위치 (인덱스에 더해짐)

public:
    friend FArchive& operator<<(FArchive& Ar, FMeshSubset& Data)
    {
        return Ar << Data.MaterialIndex 
                  << Data.IndexCount
                  << Data.StartIndexLocation
                  << Data.BaseVertexLocation;
    }
};

// 머티리얼 정보 (PBR 기반 예시)
struct FMaterialInfo
{
    FName Name; // 머티리얼 이름

    // --- 텍스처 경로 ---
    FString BaseColorMapPath;        // Albedo 또는 Diffuse 텍스처 (PBR에서는 BaseColor)
    FString NormalMapPath;           // 탄젠트 공간 노멀 맵
    FString MetallicMapPath;         // 금속성 맵 (회색조)
    FString RoughnessMapPath;        // 거칠기 맵 (회색조)
    FString EmissiveMapPath;         // 발광 맵
    FString AmbientOcclusionMapPath; // 앰비언트 오클루전 맵 (선택 사항)
    // FString SpecularMapPath;         // Specular 워크플로우용 (Metallic 워크플로우에서는 덜 직접적)

    // --- 스칼라 PBR 파라미터 (텍스처 없을 시 기본값 또는 텍스처와 곱해질 값) ---
    FLinearColor BaseColorFactor = FLinearColor::White; // 기본 색상 값 (텍스처와 곱해짐)
    float MetallicFactor = 0.0f;                        // 기본 금속성 (0: 비금속, 1: 금속)
    float RoughnessFactor = 0.5f;                       // 기본 거칠기 (0: 매끄러움, 1: 거침)
    FLinearColor EmissiveFactor = FLinearColor::Black;  // 기본 발광 색상
    float NormalMapStrength = 1.0f;                     // 노멀맵 강도
    // float Opacity = 1.0f;                               // 불투명도 (1.0: 불투명, 0.0: 완전 투명)
    // bool bIsTwoSided = false;                           // 양면 렌더링 여부

public:
    FMaterialInfo() = default; // 기본 생성자

    friend FArchive& operator<<(FArchive& Ar, FMaterialInfo& Data)
    {
        Ar << Data.Name
           << Data.BaseColorMapPath
           << Data.NormalMapPath
           << Data.MetallicMapPath
           << Data.RoughnessMapPath
           << Data.EmissiveMapPath
           << Data.AmbientOcclusionMapPath;
        // << Data.SpecularMapPath;

        Ar << Data.BaseColorFactor
           << Data.MetallicFactor
           << Data.RoughnessFactor
           << Data.EmissiveFactor
           << Data.NormalMapStrength;
        // << Data.Opacity
        // << Data.bIsTwoSided;
        return Ar;
    }
};

// 뼈 정보
struct FBoneInfo
{
    FName Name;                    // 뼈 이름 (FName으로 관리)
    int32 ParentIndex = -1;        // 부모 뼈 인덱스 (없으면 -1)
    FMatrix InverseBindPoseMatrix; // 역 바인드 포즈 행렬 (모델 공간 -> 로컬 뼈 공간)
    FMatrix LocalBindPoseMatrix;   // 로컬 바인드 포즈 행렬 (부모 뼈 기준) - 애니메이션 계산 시 유용

public:
    friend FArchive& operator<<(FArchive& Ar, FBoneInfo& Data)
    {
        return Ar << Data.Name
                  << Data.ParentIndex
                  << Data.InverseBindPoseMatrix
                  << Data.LocalBindPoseMatrix;
    }
};

// 스켈레톤 구조체
struct FSkeleton
{
    TArray<FBoneInfo> Bones;
    TMap<FName, int32> BoneNameToIndexMap; // 이름으로 뼈 인덱스 빠르게 찾기

public:
    friend FArchive& operator<<(FArchive& Ar, FSkeleton& Data)
    {
        return Ar << Data.Bones
                  << Data.BoneNameToIndexMap;
    }
};

// 애니메이션 키프레임
struct FKeyframe
{
    float TimeSeconds = 0.0f;
    FVector Translation = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    FVector Scale = FVector(1.0f);

public:
    friend FArchive& operator<<(FArchive& Ar, FKeyframe& Data)
    {
        return Ar << Data.TimeSeconds
                  << Data.Translation
                  << Data.Rotation
                  << Data.Scale;
    }
};

// 단일 뼈의 애니메이션 트랙
struct FBoneAnimation
{
    FName BoneName; // 대상 뼈 이름
    TArray<FKeyframe> Keyframes;

public:
    friend FArchive& operator<<(FArchive& Ar, FBoneAnimation& Data)
    {
        return Ar << Data.BoneName
                  << Data.Keyframes;
    }
};

// 애니메이션 클립
struct FAnimationClip
{
    FName Name;                        // 애니메이션 이름
    float DurationSeconds = 0.0f;
    float TicksPerSecond = 0.0f;       // FBX 타이밍 정보 (필요시 사용)
    TArray<FBoneAnimation> BoneTracks; // 각 뼈별 애니메이션

public:
    friend FArchive& operator<<(FArchive& Ar, FAnimationClip& Data)
    {
        return Ar << Data.Name
                  << Data.DurationSeconds
                  << Data.TicksPerSecond
                  << Data.BoneTracks;
    }
};
