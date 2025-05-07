#include "FbxImporter.h"
#include <filesystem>
#include "Components/Mesh/SkeletalMesh.h"
#include "Components/Mesh/StaticMesh.h"
#include "Engine/AssetManager.h"
#include "UObject/ObjectFactory.h"

using namespace NS_FbxTypeConverter;
namespace fs = std::filesystem;

std::shared_ptr<FFbxImporter> FFbxImporter::StaticInstance = nullptr;


namespace
{
// 헬퍼 함수: FbxProperty에서 텍스처 경로 추출
FString GetTexturePathFromProperty(const FbxProperty& Property)
{
    if (Property.IsValid() && Property.GetSrcObjectCount<FbxFileTexture>() > 0)
    {
        if (const FbxFileTexture* Texture = Property.GetSrcObject<FbxFileTexture>(0))
        {
            // GetRelativeFileName() 또는 GetFileName() 사용. 상대 경로가 더 좋을 수 있음.
            return FString{Texture->GetFileName()};
        }
    }
    return FString{};
}

// 헬퍼 함수: FbxProperty에서 FLinearColor 추출
FLinearColor GetLinearColorFromProperty(const FbxProperty& Property, const FLinearColor& DefaultValue = FLinearColor::White)
{
    if (Property.IsValid())
    {
        FbxDouble3 Color = Property.Get<FbxDouble3>();
        // FBX 색상은 보통 감마 공간일 수 있으므로, 필요시 선형 공간으로 변환해야 함.
        // 여기서는 이미 선형이라고 가정하거나, 변환 로직 추가.
        // return FLinearColor{static_cast<float>(Color[0]), static_cast<float>(Color[1]), static_cast<float>(Color[2]), 1.0f};
        return Convert<FLinearColor>(Color);
    }
    return DefaultValue;
}

// 헬퍼 함수: FbxProperty에서 float 값 추출
float GetFloatFromProperty(const FbxProperty& Property, float DefaultValue = 0.0f)
{
    if (Property.IsValid())
    {
        return static_cast<float>(Property.Get<FbxDouble>()); // 또는 FbxFloat 등
    }
    return DefaultValue;
}
}


FFbxImporter::FFbxImporter()
{
    InitializeSdk();
}

FFbxImporter::~FFbxImporter()
{
    ReleaseSdk();
}

FFbxImporter& FFbxImporter::GetInstance()
{
    if (!StaticInstance)
    {
        StaticInstance = std::shared_ptr<FFbxImporter>(new FFbxImporter());
    }
    return *StaticInstance;
}

void FFbxImporter::DeleteInstance()
{
    StaticInstance.reset();
}

bool FFbxImporter::InitializeSdk()
{
    if (SdkManager)
    {
        return true;
    }

    SdkManager = FbxManager::Create();
    if (!SdkManager)
    {
        return false;
    }

    FbxIOSettings* IOSettings = FbxIOSettings::Create(SdkManager, IOSROOT);
    SdkManager->SetIOSettings(IOSettings);

    return true;
}

void FFbxImporter::ReleaseSdk()
{
    if (CurrentScene)
    {
        CurrentScene->Destroy();
        CurrentScene = nullptr;
    }

    if (SdkManager)
    {
        SdkManager->Destroy();
        SdkManager = nullptr;
    }
}

bool FFbxImporter::ImportFromFile(const FString& InFilePath, UStaticMeshTest*& OutStaticMesh)
{
    const fs::path FilePath = InFilePath.ToWideString();

    UAssetManager& AssetManager = UAssetManager::Get();
    UStaticMeshTest* NewStaticMesh = FObjectFactory::ConstructObject<UStaticMeshTest>(&AssetManager);
    if (ImportFromFileInternal(InFilePath, NewStaticMesh, nullptr))
    {
        FAssetInfo& Info = AssetManager.GetAssetRegistry().FindOrAdd(FilePath.generic_wstring().c_str());
        Info.AssetName = FName(FilePath.filename().wstring());
        Info.AssetType = EAssetType::StaticMesh;
        Info.PackagePath = FName(FilePath.parent_path().generic_wstring());
        Info.Size = static_cast<uint32>(std::filesystem::file_size(FilePath));

        FEngineLoop::ResourceManager.AddAssignStaticMeshMap(FName(FilePath.generic_wstring()), NewStaticMesh);
        NewStaticMesh->Info = Info;
        OutStaticMesh = NewStaticMesh;
        return true;
    }
    GUObjectArray.MarkRemoveObject(NewStaticMesh);
    return false;
}

bool FFbxImporter::ImportFromFile(const FString& InFilePath, USkeletalMesh*& OutSkeletalMesh)
{
    const fs::path FilePath = InFilePath.ToWideString();

    UAssetManager& AssetManager = UAssetManager::Get();
    USkeletalMesh* NewSkeletalMesh = FObjectFactory::ConstructObject<USkeletalMesh>(&AssetManager);
    if (ImportFromFileInternal(InFilePath, nullptr, NewSkeletalMesh))
    {
        FAssetInfo& Info = AssetManager.GetAssetRegistry().FindOrAdd(FilePath.generic_wstring().c_str());
        Info.AssetName = FName(FilePath.filename().wstring());
        Info.AssetType = EAssetType::SkeletalMesh;
        Info.PackagePath = FName(FilePath.parent_path().generic_wstring());
        Info.Size = static_cast<uint32>(std::filesystem::file_size(FilePath));

        FEngineLoop::ResourceManager.AddAssignSkeletalMeshMap(FName(FilePath.generic_wstring()), NewSkeletalMesh);
        NewSkeletalMesh->Info = Info;
        OutSkeletalMesh = NewSkeletalMesh;
        return true;
    }
    GUObjectArray.MarkRemoveObject(NewSkeletalMesh);
    return false;
}

bool FFbxImporter::ImportFromFileInternal(const FString& InFilePath, UStaticMeshTest* OutStaticMesh, USkeletalMesh* OutSkeletalMesh)
{
	if (!SdkManager)
	{
		// UE_LOG(LogFbx, Error, TEXT("FBX SDK not initialized before import"));
		return false;
	}
	if ((OutStaticMesh == nullptr && OutSkeletalMesh == nullptr) || (OutStaticMesh != nullptr && OutSkeletalMesh != nullptr))
	{
		// UE_LOG(LogFbx, Error, TEXT("ImportFromFile requires exactly one of OutStaticMesh or OutSkeletalMesh to be non-null"));
		return false;
	}

	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
	bool bImportStatus = false;

	const std::string AnsiFilePathString = InFilePath.ToAnsiString();
	const char* AnsiFilePath = AnsiFilePathString.c_str();

	if (Importer->Initialize(AnsiFilePath, -1, SdkManager->GetIOSettings()))
	{
		CurrentScene = FbxScene::Create(SdkManager, "ImportScene");
		if (Importer->Import(CurrentScene))
		{
			// ** 중요: 좌표계 및 단위 변환 **
			// 엔진의 좌표계 (예: Left-Handed, Y-Up 또는 Z-Up)와 일치시킵니다.
			const FbxAxisSystem EngineAxisSystem = FbxAxisSystem::DirectX; // 또는 FbxAxisSystem::OpenGL 등
			const FbxAxisSystem SourceAxisSystem = CurrentScene->GetGlobalSettings().GetAxisSystem();
			if (SourceAxisSystem != EngineAxisSystem)
			{
				EngineAxisSystem.ConvertScene(CurrentScene);
				// UE_LOG(LogFbx, Log, TEXT("Converted scene axis system"));
			}

			// ** 중요: 단위 변환 **
			// 엔진의 단위 (예: 센티미터 또는 미터)와 일치시킵니다.
			const FbxSystemUnit EngineUnit = FbxSystemUnit::cm; // Unreal 기본 단위는 cm
			if (CurrentScene->GetGlobalSettings().GetSystemUnit() != EngineUnit)
			{
				EngineUnit.ConvertScene(CurrentScene);
				// UE_LOG(LogFbx, Log, TEXT("Converted scene system unit"));
			}

			// ** 중요: 모든 메쉬를 삼각형으로 변환 **
			// 렌더링 파이프라인은 보통 삼각형을 처리합니다.
			FbxGeometryConverter GeometryConverter(SdkManager);
			GeometryConverter.Triangulate(CurrentScene, /* bReplace */ true);
			// UE_LOG(LogFbx, Log, TEXT("Triangulated scene geometry"));

			// 노드 순회 시작
			ProcessNodeRecursive(CurrentScene->GetRootNode(), OutStaticMesh, OutSkeletalMesh);

			bImportStatus = true; // 성공 (적어도 하나 이상의 메쉬를 처리했는지 여부는 내부 로직에서 판단 가능)
		}
		else
		{
			// UE_LOG(LogFbx, Error, TEXT("Failed to import scene: %s"), ANSI_TO_TCHAR(Importer->GetStatus().GetErrorString()));
		}
		CurrentScene->Destroy(); // 임포트 후에는 씬 객체 바로 파기
		CurrentScene = nullptr;
	}
	else
	{
		// UE_LOG(LogFbx, Error, TEXT("Failed to initialize importer for file: %s"), *InFilePath);
		// UE_LOG(LogFbx, Error, TEXT("Reason: %s"), ANSI_TO_TCHAR(Importer->GetStatus().GetErrorString()));
	}

	Importer->Destroy();
	return bImportStatus;
}


void FFbxImporter::ProcessNodeRecursive(FbxNode* InNode, UStaticMeshTest* OutStaticMesh, USkeletalMesh* OutSkeletalMesh)
{
	if (!InNode) return;
	const FbxNodeAttribute* NodeAttribute = InNode->GetNodeAttribute();
	if (NodeAttribute)
	{
        const FbxNodeAttribute::EType AttributeType = NodeAttribute->GetAttributeType();
		if (AttributeType == FbxNodeAttribute::eMesh)
		{
            if (FbxMesh* Mesh = InNode->GetMesh())
			{
				// 이 노드의 메쉬 처리
				ProcessMeshNode(InNode, Mesh, OutStaticMesh, OutSkeletalMesh);
			}
		}
		// TODO: 다른 노드 타입 처리 (eSkeleton, eLight, eCamera 등) - 스켈레톤 계층 구조 빌드 시 eSkeleton 타입 활용 가능
	}

	// 자식 노드 재귀 호출
	for (int i = 0; i < InNode->GetChildCount(); ++i)
	{
		ProcessNodeRecursive(InNode->GetChild(i), OutStaticMesh, OutSkeletalMesh);
	}
}

void FFbxImporter::ProcessMeshNode(const FbxNode* InNode, FbxMesh* InMesh, UStaticMeshTest* OutStaticMesh, USkeletalMesh* OutSkeletalMesh)
{
	// 스켈레탈 메쉬인지 확인 (Skin Deformer 존재 유무)
	const bool bIsSkeletal = InMesh->GetDeformerCount(FbxDeformer::eSkin) > 0;

	if (bIsSkeletal && OutSkeletalMesh != nullptr)
	{
		// UE_LOG(LogFbx, Log, TEXT("Processing Skeletal Mesh: %s"), ANSI_TO_TCHAR(InNode->GetName()));
		ProcessSkeletalMesh(InNode, InMesh, OutSkeletalMesh);
	}
	else if (!bIsSkeletal && OutStaticMesh != nullptr)
	{
		// UE_LOG(LogFbx, Log, TEXT("Processing Static Mesh: %s"), ANSI_TO_TCHAR(InNode->GetName()));
		ProcessStaticMesh(InNode, InMesh, OutStaticMesh);
	}
	// else: 요청된 타입의 메쉬가 아니므로 건너뜀
}

void FFbxImporter::ProcessStaticMesh(const FbxNode* InNode, FbxMesh* InMesh, UStaticMeshTest* OutMesh)
{
	TArray<int32> VertexControlPointIndices; // 임시 데이터
	ExtractGeometryData<FVertex>(InMesh, OutMesh->Vertices, OutMesh->Indices, VertexControlPointIndices);

	// 스태틱 메쉬는 버텍스 수가 많을 수 있으므로, 16비트 인덱스 사용 가능 여부 체크
	// if (OutMesh->Vertices.Num() > 65535) { OutMesh->IndexFormat = DXGI_FORMAT_R32_UINT; }
	// else { OutMesh->IndexFormat = DXGI_FORMAT_R16_UINT; // TArray<uint16> 사용 필요 }

	ExtractMaterialData(InNode, InMesh, OutMesh->Materials, OutMesh->Subsets, OutMesh->Indices);

	// TODO: DX11 버퍼 생성 로직 호출
	// CreateStaticMeshResources(OutMesh);
}

void FFbxImporter::ProcessSkeletalMesh(const FbxNode* InNode, FbxMesh* InMesh, USkeletalMesh* OutMesh)
{
	TArray<int32> VertexControlPointIndices; // 각 최종 Vertex가 어떤 Control Point Index에서 왔는지 추적
	ExtractGeometryData<FSkinnedVertex>(InMesh, OutMesh->Vertices, OutMesh->Indices, VertexControlPointIndices);

	ExtractMaterialData(InNode, InMesh, OutMesh->Materials, OutMesh->Subsets, OutMesh->Indices);

	ExtractSkeletonData(InMesh, OutMesh->SkeletonData);

	ExtractSkinningData(InMesh, OutMesh->SkeletonData, VertexControlPointIndices, OutMesh->Vertices);

	DetectAnimationData(CurrentScene, OutMesh->AnimationClips); // 씬에서 애니메이션 감지
	OutMesh->bHasAnimationData = OutMesh->AnimationClips.Num() > 0;

	// TODO: DX11 버퍼 생성 로직 호출
	// CreateSkeletalMeshResources(OutMesh);
}

// 머티리얼 및 서브셋 데이터 추출
void FFbxImporter::ExtractMaterialData(
    const FbxNode* InNode, FbxMesh* InMesh,
    TArray<FMaterialInfo>& OutMaterials,
    TArray<FMeshSubset>& OutSubsets,
    TArray<uint32>& OutTriangleIndices
) const {
    const int MaterialCount = InNode->GetMaterialCount();
    OutMaterials.Empty(MaterialCount > 0 ? MaterialCount : 1);
    OutSubsets.Empty();
    OutTriangleIndices.Empty(); // 최종 인덱스 배열 초기화

    TArray<uint32> TempOriginalIndices;
    const int NumPolygons = InMesh->GetPolygonCount(); // 메쉬의 폴리곤 개수
    TempOriginalIndices.Reserve(NumPolygons * 3);

    for (int PolyIdx = 0; PolyIdx < NumPolygons; ++PolyIdx)
    {
        TempOriginalIndices.Add(static_cast<uint32>(InMesh->GetPolygonVertex(PolyIdx, 0)));
        TempOriginalIndices.Add(static_cast<uint32>(InMesh->GetPolygonVertex(PolyIdx, 1)));
        TempOriginalIndices.Add(static_cast<uint32>(InMesh->GetPolygonVertex(PolyIdx, 2)));
    }

    // 1. 머티리얼 정보 파싱
    for (int i = 0; i < MaterialCount; ++i)
    {
        FbxSurfaceMaterial* SurfaceMaterial = InNode->GetMaterial(i);
        if (!SurfaceMaterial) continue;

        FMaterialInfo NewMaterialInfo;
        NewMaterialInfo.Name = FName(SurfaceMaterial->GetName());

        // 블렌더의 Principled BSDF는 내부적으로 FbxSurfacePhong 또는 FbxSurfaceLambert로 익스포트될 수 있으며,
        // PBR 값들은 표준 프로퍼티 또는 "PBR_" 접두사가 붙은 동적 프로퍼티로 나올 수 있습니다.
        // Autodesk Standard Surface 규격을 따르는 프로퍼티를 우선적으로 찾아봅니다.

        // --- BaseColor (Albedo) ---
        FbxProperty BaseColorTexProp = SurfaceMaterial->FindProperty("Maya|baseColor_tex");                // Maya/Stingray PBR
        if (!BaseColorTexProp.IsValid()) BaseColorTexProp = SurfaceMaterial->FindProperty("DiffuseColor"); // Blender의 일반적인 Diffuse
        if (!BaseColorTexProp.IsValid()) BaseColorTexProp = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);
        NewMaterialInfo.BaseColorMapPath = GetTexturePathFromProperty(BaseColorTexProp);

        FbxProperty BaseColorFactorProp = SurfaceMaterial->FindProperty("Maya|baseColor");
        if (!BaseColorFactorProp.IsValid()) BaseColorFactorProp = SurfaceMaterial->FindProperty("DiffuseColorFactor");
        if (!BaseColorFactorProp.IsValid()) BaseColorFactorProp = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);
        NewMaterialInfo.BaseColorFactor = GetLinearColorFromProperty(BaseColorFactorProp, FLinearColor::White);

        // --- Metallic ---
        FbxProperty MetallicTexProp = SurfaceMaterial->FindProperty("Maya|metalness_tex");
        if (!MetallicTexProp.IsValid()) MetallicTexProp = SurfaceMaterial->FindProperty("MetallicMap"); // 일반적인 이름 시도
        NewMaterialInfo.MetallicMapPath = GetTexturePathFromProperty(MetallicTexProp);

        FbxProperty MetallicFactorProp = SurfaceMaterial->FindProperty("Maya|metalness");
        if (!MetallicFactorProp.IsValid()) MetallicFactorProp = SurfaceMaterial->FindProperty("MetallicFactor");
        if (!MetallicFactorProp.IsValid()) MetallicFactorProp = SurfaceMaterial->FindProperty("PBR_Metallic"); // 언리얼 FBX 임포터가 사용하는 이름
        NewMaterialInfo.MetallicFactor = GetFloatFromProperty(MetallicFactorProp, 0.0f);

        // --- Roughness ---
        FbxProperty RoughnessTexProp = SurfaceMaterial->FindProperty("Maya|specularRoughness_tex");
        if (!RoughnessTexProp.IsValid()) RoughnessTexProp = SurfaceMaterial->FindProperty("RoughnessMap");
        NewMaterialInfo.RoughnessMapPath = GetTexturePathFromProperty(RoughnessTexProp);

        FbxProperty RoughnessFactorProp = SurfaceMaterial->FindProperty("Maya|specularRoughness");
        if (!RoughnessFactorProp.IsValid()) RoughnessFactorProp = SurfaceMaterial->FindProperty("RoughnessFactor");
        if (!RoughnessFactorProp.IsValid()) RoughnessFactorProp = SurfaceMaterial->FindProperty("PBR_Roughness");
        NewMaterialInfo.RoughnessFactor = GetFloatFromProperty(RoughnessFactorProp, 0.5f);

        // Fallback: Shininess (Ns) to Roughness (Phong)
        if (NewMaterialInfo.RoughnessMapPath.IsEmpty() && FMath::IsNearlyEqual(NewMaterialInfo.RoughnessFactor, 0.5f))
        {
            FbxProperty ShininessProp = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sShininess);
            if (ShininessProp.IsValid())
            {
                float Ns = static_cast<float>(ShininessProp.Get<FbxDouble>());
                if (Ns > KINDA_SMALL_NUMBER)
                {
                    // Roughness = sqrt(1.0 - sqrt(Ns/1000.0)) for Blender
                    // หรือ Roughness = pow(1.0 - Ns/1000.0, 2.0)
                    // 다음은 일반적인 변환 중 하나:
                    NewMaterialInfo.RoughnessFactor = FMath::Sqrt(2.0f / (Ns + 2.0f));
                    NewMaterialInfo.RoughnessFactor = FMath::Clamp(NewMaterialInfo.RoughnessFactor, 0.01f, 1.0f);
                }
            }
        }

        // --- Normal Map ---
        FbxProperty NormalTexProp = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sNormalMap);
        if (!NormalTexProp.IsValid()) NormalTexProp = SurfaceMaterial->FindProperty("NormalMap"); // 일부 익스포터
        NewMaterialInfo.NormalMapPath = GetTexturePathFromProperty(NormalTexProp);

        FbxProperty BumpFactorProp = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sBumpFactor); // 노멀맵 강도
        NewMaterialInfo.NormalMapStrength = GetFloatFromProperty(BumpFactorProp, 1.0f);

        // --- Emissive ---
        FbxProperty EmissiveTexProp = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sEmissive);
        NewMaterialInfo.EmissiveMapPath = GetTexturePathFromProperty(EmissiveTexProp);

        FbxProperty EmissiveFactorProp = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sEmissiveFactor);
        NewMaterialInfo.EmissiveFactor = GetLinearColorFromProperty(EmissiveFactorProp, FLinearColor::Black);

        // --- Alpha / Opacity ---
        // 블렌더는 Alpha 입력을 사용. FBX에서는 sTransparencyFactor (0=opaque, 1=transparent) 또는
        // sTransparentColor (색상의 알파채널)로 표현될 수 있음.
        // Principled BSDF의 Alpha는 sTransparencyFactor와 반대일 수 있음.
        FbxProperty AlphaProp = SurfaceMaterial->FindProperty("Maya|opacity"); // Maya
        if (!AlphaProp.IsValid()) AlphaProp = SurfaceMaterial->FindProperty("Opacity");
        if (AlphaProp.IsValid())
        {
            NewMaterialInfo.BaseColorFactor.A = GetFloatFromProperty(AlphaProp, 1.0f);
        }
        else
        {
            FbxProperty TransparencyFactorProp = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sTransparencyFactor);
            if (TransparencyFactorProp.IsValid())
            {
                NewMaterialInfo.BaseColorFactor.A = 1.0f - GetFloatFromProperty(TransparencyFactorProp, 0.0f);
            }
        }

        // TODO: Ambient Occlusion (AO) - 블렌더에서 AO맵을 FBX로 직접 보내는 표준 방법이 명확하지 않음.
        // 보통은 BaseColor와 동일 UV를 사용하는 별도 텍스처로 준비하고, 파일명 규칙으로 찾거나,
        // glTF 익스포트 시에는 occlusionTexture로 지정됨. FBX에서는 커스텀 프로퍼티를 확인해야 할 수 있음.
        // 예시: FbxProperty AOTextureProp = SurfaceMaterial->FindProperty("OcclusionMap");
        // NewMaterialInfo.AmbientOcclusionMapPath = GetTexturePathFromProperty(AOTextureProp);

        OutMaterials.Add(NewMaterialInfo);
    }

    if (OutMaterials.IsEmpty())
    {
        FMaterialInfo DefaultMaterial;
        DefaultMaterial.Name = FName("DefaultFBXMaterial_Blender");
        // 기본 PBR 값 설정
        OutMaterials.Add(DefaultMaterial);
    }

    // 2. 서브셋 정보 생성 및 인덱스 재정렬
    FbxLayerElementMaterial* LayerMaterial = nullptr;
    if (InMesh->GetLayerCount() > 0)
    {
        LayerMaterial = InMesh->GetLayer(0)->GetMaterials();
    }

    if (!LayerMaterial || InMesh->GetPolygonCount() == 0) // 폴리곤이 없거나 머티리얼 정보 없으면
    {
        FMeshSubset Subset;
        Subset.MaterialIndex = 0;
        Subset.StartIndexLocation = 0;
        Subset.IndexCount = TempOriginalIndices.Num(); // 삼각형화된 인덱스 수
        Subset.BaseVertexLocation = 0;
        OutSubsets.Add(Subset);
        OutTriangleIndices = TempOriginalIndices; // 원본 인덱스 그대로 사용
        return;
    }

    const FbxLayerElement::EMappingMode MappingMode = LayerMaterial->GetMappingMode();
    const FbxLayerElement::EReferenceMode ReferenceMode = LayerMaterial->GetReferenceMode();
    uint32 CurrentGlobalIndexOffset = 0;

    if (MappingMode == FbxLayerElement::eAllSame)
    {
        int MaterialSlotIndex = 0; // FBX 머티리얼 슬롯 인덱스
        if (ReferenceMode == FbxLayerElement::eIndexToDirect || ReferenceMode == FbxLayerElement::eIndex)
        {
            if (LayerMaterial->GetIndexArray().GetCount() > 0)
                MaterialSlotIndex = LayerMaterial->GetIndexArray().GetAt(0);
        }
        if (MaterialSlotIndex < 0 || MaterialSlotIndex >= OutMaterials.Num()) MaterialSlotIndex = 0;

        FMeshSubset Subset;
        Subset.MaterialIndex = MaterialSlotIndex; // OutMaterials 배열의 인덱스
        Subset.StartIndexLocation = 0;
        Subset.IndexCount = TempOriginalIndices.Num();
        Subset.BaseVertexLocation = 0;
        OutSubsets.Add(Subset);
        OutTriangleIndices = TempOriginalIndices;
    }
    else if (MappingMode == FbxLayerElement::eByPolygon)
    {
        TMap<int32 /*MaterialSlotIndex*/, TArray<uint32> /*TriangleVertexIndices*/> TempMaterialToVertexIndices;
        const int PolygonCount = InMesh->GetPolygonCount(); // 각 폴리곤은 삼각형

        for (int PolyIdx = 0; PolyIdx < PolygonCount; ++PolyIdx)
        {
            int MaterialSlotIndex = 0; // 이 폴리곤에 할당된 머티리얼 슬롯 인덱스
            if (ReferenceMode == FbxLayerElement::eIndexToDirect || ReferenceMode == FbxLayerElement::eIndex)
            {
                // LayerMaterial의 인덱스 배열은 폴리곤당 하나의 머티리얼 인덱스를 가집니다.
                if (PolyIdx < LayerMaterial->GetIndexArray().GetCount())
                {
                    MaterialSlotIndex = LayerMaterial->GetIndexArray().GetAt(PolyIdx);
                }
            }

            // 가져온 MaterialSlotIndex가 OutMaterials 배열 범위 내에 있는지 확인
            if (MaterialSlotIndex < 0 || MaterialSlotIndex >= OutMaterials.Num())
            {
                MaterialSlotIndex = 0; // 유효하지 않으면 기본 머티리얼 (0번) 사용
            }

            // 현재 폴리곤(삼각형)에 해당하는 정점 인덱스 3개를 TempOriginalIndices에서 가져옵니다.
            // TempOriginalIndices는 모든 삼각형의 정점 인덱스가 순차적으로 저장되어 있습니다.
            const int32 BaseIndexInTempArray = PolyIdx * 3; // 현재 삼각형의 첫 번째 정점 인덱스가 TempOriginalIndices에서 시작하는 위치

            // TempOriginalIndices 배열의 범위를 벗어나지 않는지 확인
            if ((BaseIndexInTempArray + 2) < TempOriginalIndices.Num())
            {
                TArray<uint32>& IndicesForThisMaterialSlot = TempMaterialToVertexIndices.FindOrAdd(MaterialSlotIndex);

                // TempOriginalIndices에서 현재 삼각형의 정점 인덱스 3개를 가져와 추가
                IndicesForThisMaterialSlot.Add(TempOriginalIndices[BaseIndexInTempArray + 0]);
                IndicesForThisMaterialSlot.Add(TempOriginalIndices[BaseIndexInTempArray + 1]);
                IndicesForThisMaterialSlot.Add(TempOriginalIndices[BaseIndexInTempArray + 2]);
            }
            else
            {
                // 이 경우는 TempOriginalIndices가 잘못 채워졌거나 PolygonCount와 일치하지 않을 때 발생 가능.
                // 또는 Triangulate가 제대로 안 됐을 때 발생할 수도 있음.
                UE_LOG(
                    ELogLevel::Warning, "Index out of bounds while accessing TempOriginalIndices for polygon %d in mesh %s.",
                    PolyIdx, *FString(InMesh->GetName())
                );
            }
        }

        // 머티리얼 슬롯 인덱스 순서대로 (또는 원하는 순서대로) 서브셋 및 최종 인덱스 구성
        // TMap은 순서가 보장되지 않으므로, 필요시 정렬된 키로 순회
        TArray<int32> SortedMaterialSlots;
        TempMaterialToVertexIndices.GetKeys(SortedMaterialSlots);
        SortedMaterialSlots.Sort(); // 머티리얼 인덱스 순으로 정렬

        for (int32 MaterialSlotIndex : SortedMaterialSlots)
        {
            const TArray<uint32>* VertexIndicesPtr = TempMaterialToVertexIndices.Find(MaterialSlotIndex);
            if (VertexIndicesPtr && VertexIndicesPtr->Num() > 0)
            {
                const TArray<uint32>& VertexIndicesForThisMaterial = *VertexIndicesPtr;

                FMeshSubset Subset;
                Subset.MaterialIndex = MaterialSlotIndex; // 실제 OutMaterials 배열의 인덱스
                Subset.StartIndexLocation = CurrentGlobalIndexOffset; // 최종 인덱스 배열에서의 시작 위치
                Subset.IndexCount = VertexIndicesForThisMaterial.Num();
                Subset.BaseVertexLocation = 0; // 보통 단일 정점 버퍼를 사용하므로 0
                OutSubsets.Add(Subset);

                // 최종 인덱스 배열(OutTriangleIndices)에 현재 머티리얼의 정점 인덱스들을 추가
                OutTriangleIndices.Append(VertexIndicesForThisMaterial);
                CurrentGlobalIndexOffset += VertexIndicesForThisMaterial.Num();
            }
        }
    }
    else // 기타 지원하지 않는 모드
    {
        FMeshSubset Subset;
        Subset.MaterialIndex = 0;
        Subset.StartIndexLocation = 0;
        Subset.IndexCount = TempOriginalIndices.Num();
        Subset.BaseVertexLocation = 0;
        OutSubsets.Add(Subset);
        OutTriangleIndices = TempOriginalIndices;
    }

    OutSubsets.Shrink();
    OutTriangleIndices.Shrink(); // 최종 인덱스 배열도 최적화
}


// 스켈레톤 데이터 추출 (뼈 계층 구조 및 역 바인드 포즈)
void FFbxImporter::ExtractSkeletonData(const FbxMesh* InMesh, FSkeleton& OutSkeleton)
{
    OutSkeleton.Bones.Empty();
    OutSkeleton.BoneNameToIndexMap.Empty();

    const int DeformerCount = InMesh->GetDeformerCount(FbxDeformer::eSkin);
    if (DeformerCount == 0) return; // 스킨 정보 없음

    // 보통 하나의 스킨 디포머 사용
    FbxSkin* Skin = static_cast<FbxSkin*>(InMesh->GetDeformer(0, FbxDeformer::eSkin));
    if (!Skin) return;

    const int ClusterCount = Skin->GetClusterCount(); // Cluster = Bone Influence

    // 1단계: 모든 Cluster (뼈) 노드 정보 읽기 (계층 구조 없이)
    OutSkeleton.Bones.Reserve(ClusterCount);
    for (int i = 0; i < ClusterCount; ++i)
    {
        FbxCluster* Cluster = Skin->GetCluster(i);
        FbxNode* BoneNode = Cluster->GetLink(); // 연결된 뼈 노드
        if (!BoneNode) continue;

        FName BoneName = BoneNode->GetName();

        // 중복 추가 방지
        if (OutSkeleton.BoneNameToIndexMap.Contains(BoneName)) continue;

        FBoneInfo BoneInfo;
        BoneInfo.Name = BoneName;
        BoneInfo.ParentIndex = -1; // 아직 모름

        // 역 바인드 포즈 행렬 계산
        FbxAMatrix MeshGeometryTransform; // 메쉬의 지오메트릭 변환 (Pivot 등)
        FbxAMatrix BoneWorldBindPose;     // 바인드 포즈 시점의 뼈 월드 변환
        FbxAMatrix BoneInverseBindPoseFbx;

        Cluster->GetTransformMatrix(MeshGeometryTransform); // Mesh's transform (offset from origin)
        Cluster->GetTransformLinkMatrix(BoneWorldBindPose); // Bone's world transform at bind time

        BoneInverseBindPoseFbx = BoneWorldBindPose.Inverse() * MeshGeometryTransform;
        // BoneInfo.InverseBindPoseMatrix = ToFMatrix(BoneInverseBindPoseFbx);
        BoneInfo.InverseBindPoseMatrix = Convert<FMatrix>(BoneInverseBindPoseFbx);

        // 로컬 바인드 포즈 (참고용, 애니메이션 계산에 유용)
        BoneInfo.LocalBindPoseMatrix = Convert<FMatrix>(BoneNode->EvaluateLocalTransform());

        int32 NewBoneIndex = OutSkeleton.Bones.Add(BoneInfo);
        OutSkeleton.BoneNameToIndexMap.Add(BoneName, NewBoneIndex);
    }

    // 2단계: 스켈레톤 계층 구조 빌드 (루트부터 순회하며 부모 인덱스 설정)
    // 주의: FBX 파일에 따라서는 Cluster에 모든 뼈가 포함되지 않을 수 있음 (웨이트 없는 뼈)
    // 이상적으로는 씬 전체에서 FbxNodeAttribute::eSkeleton 타입을 찾아 계층을 빌드해야 함.
    // 여기서는 Cluster의 Link 노드 기준으로 간단히 구현
    if (CurrentScene) // CurrentScene 멤버 변수 사용
    {
        // 루트 노드부터 시작하여 뼈 노드를 찾고 부모-자식 관계 설정
        BuildSkeletonHierarchyRecursive(CurrentScene->GetRootNode(), -1, OutSkeleton);
    }
    else
    {
        // UE_LOG(LogFbx, Warning, TEXT("Cannot build full skeleton hierarchy without access to the FbxScene"));
        // Cluster에 있는 뼈들의 부모 관계만 설정 시도 (불완전할 수 있음)
        for (int32 BoneIdx = 0; BoneIdx < OutSkeleton.Bones.Num(); ++BoneIdx)
        {
            std::string TempNodeName = OutSkeleton.Bones[BoneIdx].Name.ToString().ToAnsiString();
            if (FbxNode* BoneNode = CurrentScene->FindNodeByName(TempNodeName.c_str())) // 이름으로 노드 찾기 (비효율적)
            {
                if (const FbxNode* ParentNode = BoneNode->GetParent())
                {
                    FName ParentName = ParentNode->GetName();
                    if (OutSkeleton.BoneNameToIndexMap.Contains(ParentName))
                    {
                        OutSkeleton.Bones[BoneIdx].ParentIndex = OutSkeleton.BoneNameToIndexMap[ParentName];
                    }
                }
            }
        }
    }
    OutSkeleton.Bones.Shrink();
}

void FFbxImporter::BuildSkeletonHierarchyRecursive(FbxNode* InNode, int32 InParentBoneIndex, FSkeleton& OutSkeleton)
{
    if (!InNode) return;

    const FName CurrentNodeName = InNode->GetName();
    int32 CurrentBoneIndex = -1;

    // 이 노드가 스켈레톤의 뼈인지 확인하고, 부모 인덱스 설정
    if (OutSkeleton.BoneNameToIndexMap.Contains(CurrentNodeName))
    {
        CurrentBoneIndex = OutSkeleton.BoneNameToIndexMap[CurrentNodeName];
        OutSkeleton.Bones[CurrentBoneIndex].ParentIndex = InParentBoneIndex;

        // 로컬 바인드 포즈 재계산 (EvaluateLocalTransform 사용)
        OutSkeleton.Bones[CurrentBoneIndex].LocalBindPoseMatrix = Convert<FMatrix>(InNode->EvaluateLocalTransform());
    }

    // 자식 노드 재귀 호출
    for (int i = 0; i < InNode->GetChildCount(); ++i)
    {
        // 자식 노드 처리 시, 현재 노드가 뼈였다면 그 인덱스를 부모 인덱스로 전달
        BuildSkeletonHierarchyRecursive(InNode->GetChild(i), CurrentBoneIndex, OutSkeleton);
    }
}

// 스킨 웨이트 데이터 추출 및 버텍스에 할당
void FFbxImporter::ExtractSkinningData(const FbxMesh* InMesh, const FSkeleton& InSkeleton, const TArray<int32>& InVertexControlPointIndices, TArray<FSkinnedVertex>& OutVertices) const
{
	const int DeformerCount = InMesh->GetDeformerCount(FbxDeformer::eSkin);
	if (DeformerCount == 0 || OutVertices.IsEmpty() || InSkeleton.Bones.IsEmpty()) return;

	FbxSkin* Skin = static_cast<FbxSkin*>(InMesh->GetDeformer(0, FbxDeformer::eSkin));
	if (!Skin) return;

	const int ClusterCount = Skin->GetClusterCount();

	// 각 Control Point에 영향을 주는 (BoneIndex, Weight) 임시 저장
	TArray<TArray<TPair<int32, float>>> ControlPointWeights;
	ControlPointWeights.AddDefaulted(InMesh->GetControlPointsCount()); // Control Point 개수만큼 초기화

	for (int ClusterIdx = 0; ClusterIdx < ClusterCount; ++ClusterIdx)
	{
		FbxCluster* Cluster = Skin->GetCluster(ClusterIdx);
		const FbxNode* BoneNode = Cluster->GetLink();
		if (!BoneNode) continue;

		FName BoneName = BoneNode->GetName();
		if (!InSkeleton.BoneNameToIndexMap.Contains(BoneName)) continue; // 스켈레톤에 없는 뼈 무시

		int32 BoneIndex = InSkeleton.BoneNameToIndexMap[BoneName];
		const int* AffectedControlPointIndices = Cluster->GetControlPointIndices();
		const double* Weights = Cluster->GetControlPointWeights();
		const int NumAffectedControlPoints = Cluster->GetControlPointIndicesCount();

		for (int i = 0; i < NumAffectedControlPoints; ++i)
		{
			const int ControlPointIndex = AffectedControlPointIndices[i];
			float Weight = static_cast<float>(Weights[i]);

			if (Weight > KINDA_SMALL_NUMBER) // 거의 0인 가중치는 무시
			{
				ControlPointWeights[ControlPointIndex].Add(TPair<int32, float>(BoneIndex, Weight));
			}
		}
	}

	// 최종 버텍스 데이터에 가중치 할당
	assert(OutVertices.Num() == InVertexControlPointIndices.Num()); // 개수가 같아야 함

	for (int VertexIdx = 0; VertexIdx < OutVertices.Num(); ++VertexIdx)
	{
		const int ControlPointIndex = InVertexControlPointIndices[VertexIdx];
		FSkinnedVertex& Vertex = OutVertices[VertexIdx]; // 레퍼런스로 가져옴

		const TArray<TPair<int32, float>>& WeightsForThisVertex = ControlPointWeights[ControlPointIndex];

		// 가중치 높은 순으로 정렬
		TArray<TPair<int32, float>> SortedWeights = WeightsForThisVertex;
		SortedWeights.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B) {
			return A.Value > B.Value; // Weight 내림차순
		});

		float TotalWeight = 0.0f;
		const int NumWeightsToUse = FMath::Min(SortedWeights.Num(), 4); // 최대 4개 사용

		for (int i = 0; i < NumWeightsToUse; ++i)
		{
			Vertex.BoneIndices[i] = static_cast<uint8>(SortedWeights[i].Key); // 인덱스 저장 (uint8 캐스팅 주의)
			Vertex.BoneWeights[i] = SortedWeights[i].Value;
			TotalWeight += SortedWeights[i].Value;
		}

		// 가중치 정규화 (합이 1이 되도록)
		if (TotalWeight > KINDA_SMALL_NUMBER)
		{
			for (int i = 0; i < NumWeightsToUse; ++i)
			{
				Vertex.BoneWeights[i] /= TotalWeight;
			}
		}
		else // 가중치 합이 0이면 (오류 또는 웨이트 없는 버텍스)
		{
		    if(NumWeightsToUse > 0) {
				// 첫 번째 뼈에 1.0 할당 또는 루트 뼈에 할당
				Vertex.BoneWeights[0] = 1.0f;
				for (int i = 1; i < 4; ++i) Vertex.BoneWeights[i] = 0.0f;
			} else {
                // 아예 웨이트 정보가 없는 경우, 보통 루트 뼈(0번)에 1.0 할당
                 Vertex.BoneIndices[0] = 0;
                 Vertex.BoneWeights[0] = 1.0f;
            }
		}

		// 남은 슬롯 초기화 (가중치 0으로)
		for (int i = NumWeightsToUse; i < 4; ++i)
		{
			Vertex.BoneIndices[i] = 0; // 보통 루트 뼈 인덱스
			Vertex.BoneWeights[i] = 0.0f;
		}
	}
}


// 애니메이션 데이터 감지 (기본 정보만 로드)
void FFbxImporter::DetectAnimationData(FbxScene* InScene, TArray<FAnimationClip>& OutAnimationClips) const
{
	OutAnimationClips.Empty();
	if (!InScene) return;

	// FBX는 여러 애니메이션 스택(클립)을 가질 수 있음
	const int NumAnimStacks = InScene->GetSrcObjectCount<FbxAnimStack>();

	for (int i = 0; i < NumAnimStacks; ++i)
	{
		const FbxAnimStack* AnimStack = InScene->GetSrcObject<FbxAnimStack>(i);
		if (!AnimStack) continue;

		FAnimationClip ClipInfo;
		ClipInfo.Name = AnimStack->GetName();

		// 애니메이션 길이 및 타이밍 정보 가져오기 (TakeInfo 사용)
		const FbxTakeInfo* TakeInfo = InScene->GetTakeInfo(AnimStack->GetName());
		if (TakeInfo)
		{
			FbxTime StartTime = TakeInfo->mLocalTimeSpan.GetStart();
			FbxTime StopTime = TakeInfo->mLocalTimeSpan.GetStop();
			ClipInfo.DurationSeconds = static_cast<float>(StopTime.GetSecondDouble() - StartTime.GetSecondDouble());
			ClipInfo.TicksPerSecond = static_cast<float>(FbxTime::GetFrameRate(InScene->GetGlobalSettings().GetTimeMode()));
		}
		else
		{
			// TakeInfo가 없으면 AnimCurve에서 직접 계산해야 함 (복잡)
			ClipInfo.DurationSeconds = 0.0f; // 알 수 없음
			ClipInfo.TicksPerSecond = static_cast<float>(FbxTime::GetFrameRate(InScene->GetGlobalSettings().GetTimeMode()));
		}

		// --- 실제 키프레임 데이터 로드는 나중에 구현 ---
		// AnimStack -> AnimLayer -> AnimCurveNode (뼈별 변환) -> AnimCurve (T, R, S) 순으로 탐색 필요
		// 각 뼈의 Transform(T, R, S) 커브를 찾아 FKeyframe 데이터를 채워야 함.
		// 예시:
		// int NumLayers = AnimStack->GetMemberCount<FbxAnimLayer>();
		// for (int LayerIdx = 0; LayerIdx < NumLayers; ++LayerIdx) {
		//     FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(LayerIdx);
		//     // ... 이 레이어에서 스켈레톤의 각 뼈 노드를 찾아 해당 노드의 T, R, S AnimCurveNode를 찾음 ...
		//     // ... AnimCurveNode에서 실제 AnimCurve를 얻고, 키프레임(시간, 값)을 읽어 FKeyframe으로 변환 ...
		// }
		// ClipInfo.BoneTracks.Add(...); // 채워진 뼈 애니메이션 트랙 추가

		OutAnimationClips.Add(ClipInfo);
	}
	OutAnimationClips.Shrink();
}
