#pragma once
#include <filesystem>

#include "Define.h"
#include "EngineLoop.h"

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

    FObjMaterialInfo ConvertObjMaterialInfo() const
    {
        FObjMaterialInfo MatInfo;

        // 1. 이름 변환
        MatInfo.MaterialName = Name.ToString();

        // 2. PBR 파라미터를 전통적인 파라미터로 변환
        bool bIsMetallic = MetallicFactor >= 0.5f; // 금속성 여부 (텍스처 없을 시)

        // DiffuseColor (Kd)
        if (bIsMetallic && MetallicMapPath.IsEmpty()) // 텍스처 없는 금속
        {
            MatInfo.DiffuseColor = FVector(0.0f, 0.0f, 0.0f); // 금속은 보통 검은색 Diffuse
        }
        else // 비금속 또는 Diffuse/BaseColor 텍스처 사용 시
        {
            MatInfo.DiffuseColor = FVector(BaseColorFactor.R, BaseColorFactor.G, BaseColorFactor.B);
        }

        // SpecularColor (Ks)
        if (bIsMetallic && MetallicMapPath.IsEmpty()) // 텍스처 없는 금속
        {
            MatInfo.SpecularColor = FVector(BaseColorFactor.R, BaseColorFactor.G, BaseColorFactor.B); // 금속의 Specular는 BaseColor
        }
        else // 비금속
        {
            // 비금속의 경우 일반적인 F0 값 (0.04) 또는 IOR 기반으로 설정 가능
            // 여기서는 단순하게 MetallicFactor를 반영하여 설정 (완전 비금속은 낮게, 약간 금속성 있으면 조금 더 높게)
            float NonMetalSpecularIntensity = FMath::Lerp(0.04f, 0.2f, MetallicFactor * 2.0f); // MetallicFactor가 0.5 미만일 때를 고려
            MatInfo.SpecularColor = FVector(NonMetalSpecularIntensity);
        }

        // AmbientColor (Ka)
        // PBR에서는 AO맵이 주로 이 역할을 하며, Ka는 종종 Kd의 어두운 버전이나 Kd와 같게 설정됨
        // 여기서는 Diffuse의 일부로 하거나, AO 맵이 있으면 해당 맵을 map_Ka로 연결
        MatInfo.AmbientColor = MatInfo.DiffuseColor * 0.1f; // 간단한 예시

        // EmissiveColor (Ke)
        MatInfo.EmissiveColor = FVector(EmissiveFactor.R, EmissiveFactor.G, EmissiveFactor.B);

        // SpecularExponent (Ns) - Roughness와 반비례
        // Roughness가 0이면 매우 반짝 (Ns 높음), 1이면 매우 거침 (Ns 낮음)
        // 이 공식은 근사치이며, 필요에 따라 조정해야 합니다.
        MatInfo.SpecularExponent = FMath::Max(2.f, (1.f - RoughnessFactor) * (1.f - RoughnessFactor) * 1000.f);
        if (!RoughnessMapPath.IsEmpty()) // 거칠기 맵이 있으면 Ns를 매우 높게 설정해서 맵이 주도하도록 유도 (또는 map_Ns 지원 확인)
        {
            // OBJ는 map_Ns (Roughness map에 대한 직접적인 스펙큘러 지수 맵)을 지원하기도 함.
            // 여기서는 Roughness 맵이 있으면 Exponent를 일단 기본값보다 높게 설정.
            MatInfo.SpecularExponent = 250.f; // 기본값 또는 다른 값으로 설정
        }

        // IOR (Ni)
        MatInfo.IOR = 1.5f; // 일반적인 비금속 값, 금속의 경우 더 복잡하지만 OBJ에서는 주로 비금속에 사용

        // Transparency (d or Tr) and bTransparent
        // OBJ의 'd'는 불투명도 (1=불투명, 0=완전투명). 'Tr'은 투명도 (0=불투명, 1=완전투명). 여기서는 'd'를 기준으로.
        // ObjMatInfo.Transparency = Opacity; // FMaterialInfo의 Opacity를 d로 사용
        // if (Opacity < 0.99f) // 거의 완전 불투명이 아니면 투명하다고 간주
        // {
        //     ObjMatInfo.bTransparent = true;
        //     // 투명 재질의 경우 조명 모델을 다르게 설정할 수 있음 (예: 4, 6, 7, 9)
        //     // ObjMatInfo.IlluminanceModel = 4; // 예시: Ray trace on, Transparency: Glass on
        // }

        MatInfo.Transparency = 1.0f;
        MatInfo.bTransparent = false;

        // Metallic & Roughness (PBR 확장 파라미터, OBJ 표준 아님)
        MatInfo.Metallic = MetallicFactor;
        MatInfo.Roughness = RoughnessFactor;

        // 3. 텍스처 정보 변환
        static auto FindOrLoad = [](const std::filesystem::path& FileName, bool bIsSRGB)
        {
            if (FEngineLoop::ResourceManager.GetTexture(FileName.generic_wstring()))
            {
                return;
            }

            FEngineLoop::ResourceManager.LoadTextureFromFile(FEngineLoop::GraphicDevice.Device, FileName.generic_wstring().c_str(), bIsSRGB);
        };

        constexpr uint32 TexturesNum = static_cast<uint32>(EMaterialTextureSlots::MTS_MAX);
        MatInfo.TextureInfos.SetNum(TexturesNum);

        if (!BaseColorMapPath.IsEmpty())
        {
            constexpr uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Diffuse);
            std::filesystem::path TempBaseColorMapPath = BaseColorMapPath.ToWideString();
            FindOrLoad(TempBaseColorMapPath, true);
            MatInfo.TextureInfos[SlotIdx] = {
                .TextureName = TempBaseColorMapPath.filename(),
                .TexturePath = TempBaseColorMapPath.generic_wstring(),
                .bIsSRGB = true
            }; // Diffuse map (sRGB)
            MatInfo.TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Diffuse);
        }
        if (!NormalMapPath.IsEmpty())
        {
            // OBJ 표준은 map_Bump 또는 단순히 bump. 일부 로더는 norm도 인식.
            constexpr uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Normal);
            std::filesystem::path TempNormalMapPath = NormalMapPath.ToWideString();
            FindOrLoad(TempNormalMapPath, false);
            MatInfo.TextureInfos[SlotIdx] = {
                .TextureName = TempNormalMapPath.filename(),
                .TexturePath = TempNormalMapPath.generic_wstring(),
                .bIsSRGB = false
            }; // Normal map (Linear)
            MatInfo.TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Normal);
            MatInfo.BumpMultiplier = NormalMapStrength; // 노멀맵 강도를 BumpMultiplier로 사용
        }
        if (!MetallicMapPath.IsEmpty())
        {
            // OBJ 표준에는 Metallic map (map_Pm)이 명확히 없음. PBR 확장.
            constexpr uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Metallic);
            std::filesystem::path TempMetallicMapPath = MetallicMapPath.ToWideString();
            FindOrLoad(TempMetallicMapPath, false);
            MatInfo.TextureInfos[SlotIdx] = {
                .TextureName = TempMetallicMapPath.filename(),
                .TexturePath = TempMetallicMapPath.generic_wstring(),
                .bIsSRGB = false
            }; // Metallic map (Linear)
            MatInfo.TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Metallic);
        }
        if (!RoughnessMapPath.IsEmpty())
        {
            // OBJ 표준에는 Roughness map (map_Pr)이 명확히 없음. PBR 확장.
            // 일부는 map_Ns (스펙큘러 지수 맵) 또는 map_Sharpness를 사용.
            constexpr uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Roughness);
            std::filesystem::path TempRoughnessMapPath = RoughnessMapPath.ToWideString();
            FindOrLoad(TempRoughnessMapPath, false);
            MatInfo.TextureInfos[SlotIdx] = {
                .TextureName = TempRoughnessMapPath.filename(),
                .TexturePath = TempRoughnessMapPath.generic_wstring(),
                .bIsSRGB = false
            }; // Roughness map (Linear)
            MatInfo.TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Roughness);
        }
        if (!EmissiveMapPath.IsEmpty())
        {
            constexpr uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Emissive);
            std::filesystem::path TempEmissiveMapPath = EmissiveMapPath.ToWideString();
            FindOrLoad(TempEmissiveMapPath, true);
            MatInfo.TextureInfos[SlotIdx] = {
                .TextureName = TempEmissiveMapPath.filename(),
                .TexturePath = TempEmissiveMapPath.generic_wstring(),
                .bIsSRGB = true
            }; // Emissive map (sRGB)
            MatInfo.TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Emissive);
        }
        if (!AmbientOcclusionMapPath.IsEmpty())
        {
            // AO맵은 map_Ka (Ambient Color map)으로 연결하거나 별도 map_ao 등으로 사용.
            constexpr uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Ambient);
            std::filesystem::path TempAmbientOcclusionMapPath = AmbientOcclusionMapPath.ToWideString();
            FindOrLoad(TempAmbientOcclusionMapPath, false);
            MatInfo.TextureInfos[SlotIdx] = {
                .TextureName = TempAmbientOcclusionMapPath.filename(),
                .TexturePath = TempAmbientOcclusionMapPath.generic_wstring(),
                .bIsSRGB = false
            }; // AO map (Linear)
            MatInfo.TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Ambient);
        }
        // SpecularMapPath 처리 (주석 처리됨)
        // if (!SpecularMapPath.IsEmpty())
        // {
        //     constexpr uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Specular);
        //     std::filesystem::path TempSpecularMapPath = SpecularMapPath.ToWideString();
        //     FindOrLoad(TempSpecularMapPath, false);
        //     ObjMatInfo.TextureInfos.Add({
        //         .TextureName = TempSpecularMapPath.filename(),
        //         .TexturePath = TempSpecularMapPath.generic_wstring(),
        //         .bIsSRGB = false
        //     }); // Specular color map (Linear)
        //     ObjMatInfo.TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Specular);
        // }

        MatInfo.IlluminanceModel = 2;

        return MatInfo;
    }

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
