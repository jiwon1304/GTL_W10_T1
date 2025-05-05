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
};

// 스켈레탈 메쉬용 정점 구조체
struct FSkinnedVertex : public FVertex
{
    // 최대 4개의 뼈 영향 가정 (DX11 Input Layout 시맨틱: BLENDINDICES, BLENDWEIGHTS)
    uint8 BoneIndices[4] = { 0, 0, 0, 0 };
    float BoneWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // 합은 1.0이 되도록 정규화
};

// 단일 드로우 콜에 해당하는 메쉬의 일부 (머티리얼별로 나뉨)
struct FMeshSubset
{
    int32 MaterialIndex = -1;      // 이 서브셋에 적용될 머티리얼 인덱스
    uint32 IndexCount = 0;         // 이 서브셋을 그리는 데 사용될 인덱스 개수
    uint32 StartIndexLocation = 0; // 전체 인덱스 버퍼에서의 시작 위치
    int32 BaseVertexLocation = 0;  // 전체 버텍스 버퍼에서의 시작 위치 (인덱스에 더해짐)
};

// 머티리얼 정보 (PBR 기반 예시)
// TODO: 기본 Material 만들기
struct FMaterialInfo
{
    FName Name;
    FString DiffuseMapPath;
    FString NormalMapPath;
    FString SpecularMapPath;
    FString RoughnessMapPath;
    FString MetallicMapPath;
    FString EmissiveMapPath;
    FLinearColor DiffuseAlbedo;
};

// 뼈 정보
struct FBoneInfo
{
    FName Name;                    // 뼈 이름 (FName으로 관리)
    int32 ParentIndex = -1;        // 부모 뼈 인덱스 (없으면 -1)
    FMatrix InverseBindPoseMatrix; // 역 바인드 포즈 행렬 (모델 공간 -> 로컬 뼈 공간)
    FMatrix LocalBindPoseMatrix;   // 로컬 바인드 포즈 행렬 (부모 뼈 기준) - 애니메이션 계산 시 유용
};

// 스켈레톤 구조체
struct FSkeleton
{
    TArray<FBoneInfo> Bones;
    TMap<FName, int32> BoneNameToIndexMap; // 이름으로 뼈 인덱스 빠르게 찾기
};

// 애니메이션 키프레임
struct FKeyframe
{
    float TimeSeconds = 0.0f;
    FVector Translation = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    FVector Scale = FVector(1.0f);
};

// 단일 뼈의 애니메이션 트랙
struct FBoneAnimation
{
    FName BoneName; // 대상 뼈 이름
    TArray<FKeyframe> Keyframes;
};

// 애니메이션 클립
struct FAnimationClip
{
    FName Name;                        // 애니메이션 이름
    float DurationSeconds = 0.0f;
    float TicksPerSecond = 0.0f;       // FBX 타이밍 정보 (필요시 사용)
    TArray<FBoneAnimation> BoneTracks; // 각 뼈별 애니메이션
};
