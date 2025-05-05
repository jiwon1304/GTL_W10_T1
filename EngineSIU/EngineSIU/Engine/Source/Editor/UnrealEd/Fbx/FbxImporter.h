#pragma once
#include <fbxsdk.h>

#include "EngineMeshTypes.h"
#include "Container/Array.h"

class USkeletalMesh;
class UStaticMeshTest;
class FString;

namespace NS_FbxTypeConverter
{
template <typename To, typename From, size_t... Indices>
To ConvertFromIndexableHelper(const From& V, std::index_sequence<Indices...>)
{
    return To{ static_cast<float>(V[Indices])... };
}

template <typename To, typename From>
// ReSharper disable once CppStaticAssertFailure
To Convert(const From&) { static_assert(false, "Invalid type"); return To{}; }

template <typename To, typename From>
    requires requires(const From& V) { V.mData; }
To Convert(const From& Val)
{
    constexpr size_t NumElements = std::size(Val.mData);
    return ConvertFromIndexableHelper<To>(Val, std::make_index_sequence<NumElements>{});
}

template <>
inline FMatrix Convert<FMatrix, FbxAMatrix>(const FbxAMatrix& Val)
{
    FMatrix Result;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            Result.M[i][j] = static_cast<float>(Val.Get(i, j));
    return Result;
}

template <>
inline FVector Convert<FVector, FbxVector4>(const FbxVector4& Val)
{
    constexpr size_t NumElements = 3; // W는 무시
    return ConvertFromIndexableHelper<FVector>(Val, std::make_index_sequence<NumElements>{});
}
}

class FFbxImporter
{
    static std::shared_ptr<FFbxImporter> StaticInstance;
    FFbxImporter();

public:
    ~FFbxImporter();

    static FFbxImporter& GetInstance();
    static void DeleteInstance();

    FFbxImporter(const FFbxImporter&) = delete;
    FFbxImporter& operator=(const FFbxImporter&) = delete;
    FFbxImporter(FFbxImporter&&) = delete;
    FFbxImporter& operator=(FFbxImporter&&) = delete;

public:
    bool InitializeSdk();
    void ReleaseSdk();

    // 파일을 로드하여 StaticMesh 또는 SkeletalMesh 에셋을 채웁니다.
    // 둘 중 하나만 null이 아니어야 합니다.
    bool ImportFromFile(const FString& InFilePath, UStaticMeshTest* OutStaticMesh, USkeletalMesh* OutSkeletalMesh);

private:
    // 씬 그래프 순회 및 처리
    void ProcessNodeRecursive(FbxNode* InNode, UStaticMeshTest* OutStaticMesh, USkeletalMesh* OutSkeletalMesh);
    void ProcessMeshNode(const FbxNode* InNode, FbxMesh* InMesh, UStaticMeshTest* OutStaticMesh, USkeletalMesh* OutSkeletalMesh);

    // 메쉬 데이터 추출 로직
    void ProcessStaticMesh(const FbxNode* InNode, FbxMesh* InMesh, UStaticMeshTest* OutMesh);
    void ProcessSkeletalMesh(const FbxNode* InNode, FbxMesh* InMesh, USkeletalMesh* OutMesh);

    // 공통 추출 함수
    template <typename VertexType>
    void ExtractGeometryData(
        FbxMesh* InMesh, TArray<VertexType>& OutVertices, TArray<uint32>& OutIndices, TArray<int32>& OutVertexControlPointIndices
    ); // 각 Vertex가 어떤 Control Point에서 왔는지 추적

    void ExtractMaterialData(
        const FbxNode* InNode, FbxMesh* InMesh, TArray<FMaterialInfo>& OutMaterials, TArray<FMeshSubset>& OutSubsets, const TArray<uint32>& InIndices
    ) const;

    void ExtractSkeletonData(const FbxMesh* InMesh, FSkeleton& OutSkeleton);
    void BuildSkeletonHierarchyRecursive(FbxNode* InNode, int32 InParentBoneIndex, FSkeleton& OutSkeleton);

    void ExtractSkinningData(
        const FbxMesh* InMesh, const FSkeleton& InSkeleton, const TArray<int32>& InVertexControlPointIndices, TArray<FSkinnedVertex>& OutVertices
    ) const;

    void DetectAnimationData(FbxScene* InScene, TArray<FAnimationClip>& OutAnimationClips) const;

    // FBX Layer Element 데이터 추출 헬퍼
    template <typename TDataType>
    bool GetVertexElement(
        FbxMesh* InMesh, FbxLayerElement::EType InElementType, int InPolygonIndex, int InPolygonVertexIndex, int InControlPointIndex,
        TDataType& OutValue, int InElementIndex = 0
    );

    // 버텍스 비교 헬퍼 (중복 제거용)
    template <typename VertexType>
    bool AreVerticesEqual(const VertexType& V1, const VertexType& V2);

private:
    FbxManager* SdkManager = nullptr;
    FbxScene* CurrentScene = nullptr;
};

// 지오메트리 데이터 추출 (정점, 인덱스, ControlPoint 매핑)
template <typename VertexType>
void FFbxImporter::ExtractGeometryData(
    FbxMesh* InMesh, TArray<VertexType>& OutVertices, TArray<uint32>& OutIndices, TArray<int32>& OutVertexControlPointIndices
)
{
    const int PolygonCount = InMesh->GetPolygonCount();
    const FbxVector4* ControlPoints = InMesh->GetControlPoints();
    const int ControlPointCount = InMesh->GetControlPointsCount();

    OutVertices.Empty();
    OutIndices.Empty();
    OutVertexControlPointIndices.Empty();
    OutIndices.Reserve(PolygonCount * 3); // 삼각형 당 3개 인덱스 예상

    // 중복 버텍스 방지를 위한 맵핑: ControlPointIndex -> TArray<생성된 Vertex Index>
    TMap<int32, TArray<int32>> ControlPointToVertexIndexMap;
    ControlPointToVertexIndexMap.Reserve(ControlPointCount);

    uint32 CurrentVertexIndex = 0;

    for (int i = 0; i < PolygonCount; ++i)
    {
        // Triangulate 했으므로 항상 3
        assert(InMesh->GetPolygonSize(i) == 3);

        for (int j = 0; j < 3; ++j)
        {
            const int ControlPointIndex = InMesh->GetPolygonVertex(i, j);

            VertexType CurrentVertex;
            CurrentVertex.Position = NS_FbxTypeConverter::Convert<FVector>(ControlPoints[ControlPointIndex]);

            // Normal, UV, Tangent 등 Layer Element에서 데이터 추출
            FbxVector4 TempNormal;
            if (GetVertexElement(InMesh, FbxLayerElement::eNormal, i, j, ControlPointIndex, TempNormal))
            {
                CurrentVertex.Normal = NS_FbxTypeConverter::Convert<FVector>(TempNormal).GetSafeNormal(); // 정규화
            }
            else { CurrentVertex.Normal = FVector::ZeroVector; /* 기본값 또는 오류 처리 */ }

            FbxVector2 TempUV;
            if (GetVertexElement(InMesh, FbxLayerElement::eUV, i, j, ControlPointIndex, TempUV, 0 /* UV Set 0 */))
            {
                CurrentVertex.TexCoord = NS_FbxTypeConverter::Convert<FVector2D>(TempUV);
                CurrentVertex.TexCoord.Y = 1.0f - CurrentVertex.TexCoord.Y; // DX 스타일 (Top-Left)로 변환 (필요시)
            }
            else { CurrentVertex.TexCoord = FVector2D::ZeroVector; }

            FbxVector4 TempTangent;
            if (GetVertexElement(InMesh, FbxLayerElement::eTangent, i, j, ControlPointIndex, TempTangent, 0 /* Tangent Set 0 */))
            {
                CurrentVertex.Tangent = NS_FbxTypeConverter::Convert<FVector>(TempTangent).GetSafeNormal(); // 정규화
            }
            else { CurrentVertex.Tangent = FVector::ZeroVector; /* TODO: 필요시 Normal, UV로 계산 */ }

            // --- 중복 버텍스 검사 ---
            bool bFoundDuplicate = false;
            int32 ExistingVertexIndex = -1;

            if (ControlPointToVertexIndexMap.Contains(ControlPointIndex))
            {
                const TArray<int32>& PotentialDuplicates = ControlPointToVertexIndexMap[ControlPointIndex];
                for (int32 VertexIdxToCheck : PotentialDuplicates)
                {
                    // Position은 같으므로 Normal, UV, Tangent 등 비교
                    if (AreVerticesEqual(OutVertices[VertexIdxToCheck], CurrentVertex))
                    {
                        bFoundDuplicate = true;
                        ExistingVertexIndex = VertexIdxToCheck;
                        break;
                    }
                }
            }

            if (bFoundDuplicate)
            {
                // 기존 버텍스 인덱스 사용
                OutIndices.Add(ExistingVertexIndex);
            }
            else
            {
                // 새 버텍스 추가
                OutVertices.Add(CurrentVertex);
                OutIndices.Add(CurrentVertexIndex);
                OutVertexControlPointIndices.Add(ControlPointIndex); // 새 버텍스와 ControlPoint 인덱스 매핑 저장

                // ControlPoint -> Vertex Index 맵 업데이트
                ControlPointToVertexIndexMap.FindOrAdd(ControlPointIndex).Add(static_cast<int32>(CurrentVertexIndex));

                ++CurrentVertexIndex;
            }
        }
    }

    OutVertices.Shrink(); // 메모리 최적화
    OutIndices.Shrink();
    OutVertexControlPointIndices.Shrink();
}

template <typename TDataType>
bool FFbxImporter::GetVertexElement(
    FbxMesh* InMesh, FbxLayerElement::EType InElementType, int InPolygonIndex, int InPolygonVertexIndex, int InControlPointIndex, TDataType& OutValue,
    int InElementIndex
)
{
    if (!InMesh || !InMesh->GetLayer(0)) return false; // 기본 레이어 0 가정

    FbxLayerElementTemplate<TDataType>* Element = static_cast<FbxLayerElementTemplate<TDataType>*>(
        InMesh->GetLayer(0)
              ->GetLayerElementOfType(InElementType, InElementIndex)
    );

    if (!Element) return false; // 해당 타입 요소 없음

    const FbxLayerElement::EMappingMode MappingMode = Element->GetMappingMode();
    const FbxLayerElement::EReferenceMode ReferenceMode = Element->GetReferenceMode();
    int DirectIndex = -1;

    switch (MappingMode)
    {
    case FbxLayerElement::eByControlPoint:
        switch (ReferenceMode)
        {
        case FbxLayerElement::eDirect:
            DirectIndex = InControlPointIndex;
            break;
        case FbxLayerElement::eIndexToDirect:
        case FbxLayerElement::eIndex: // FBX SDK 문서상 eIndex는 eIndexToDirect와 유사하게 동작할 수 있음
            if (InControlPointIndex < Element->GetIndexArray().GetCount())
                DirectIndex = Element->GetIndexArray().GetAt(InControlPointIndex);
            break;
        default: break;
        }
        break;

    case FbxLayerElement::eByPolygonVertex:
    {
        int IndexByPolygonVertex = InPolygonIndex * 3 + InPolygonVertexIndex; // 삼각형 기준 인덱스

        // UV는 특별한 인덱스 함수를 가질 수 있음 (구버전 호환성 등)
        if (InElementType == FbxLayerElement::eUV)
        {
            // FBX SDK 버전에 따라 GetTextureUVIndex가 없을 수 있음. 기본 계산 방식 사용.
            // int uvIndex = InMesh->GetTextureUVIndex(InPolygonIndex, InPolygonVertexIndex);
            // IndexByPolygonVertex = uvIndex; // 만약 위 함수가 있다면 사용
        }

        switch (ReferenceMode)
        {
        case FbxLayerElement::eDirect:
            DirectIndex = IndexByPolygonVertex;
            break;
        case FbxLayerElement::eIndexToDirect:
        case FbxLayerElement::eIndex:
            if (IndexByPolygonVertex < Element->GetIndexArray().GetCount())
                DirectIndex = Element->GetIndexArray().GetAt(IndexByPolygonVertex);
            break;
        default: break;
        }
    }
    break;

    // TODO: 다른 MappingMode (eByPolygon, eAllSame 등) 필요한 경우 처리
    default:
        // UE_LOG(LogFbx, Warning, TEXT("Unsupported MappingMode: %d for element type %d"), MappingMode, InElementType);
        break;
    }

    if (DirectIndex != -1 && DirectIndex < Element->GetDirectArray().GetCount())
    {
        OutValue = Element->GetDirectArray().GetAt(DirectIndex);
        return true;
    }

    return false;
}

template <typename VertexType>
bool FFbxImporter::AreVerticesEqual(const VertexType& V1, const VertexType& V2)
{
    // Position은 이미 같다고 가정 (ControlPointIndex 기반 비교)
    // Normal, UV, Tangent 등 비교 (오차 범위 고려)
    constexpr float Epsilon = KINDA_SMALL_NUMBER; // 적절한 오차값 설정
    const bool bNormalEqual = V1.Normal.Equals(V2.Normal, Epsilon);
    const bool bUVEqual = V1.TexCoord.Equals(V2.TexCoord, Epsilon);
    const bool bTangentEqual = V1.Tangent.Equals(V2.Tangent, Epsilon);
    // TODO: 필요시 Bitangent, Vertex Color 등 추가 비교

    return bNormalEqual && bUVEqual && bTangentEqual;
}
