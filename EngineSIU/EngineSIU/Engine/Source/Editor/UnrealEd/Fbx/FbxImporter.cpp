#include "FbxImporter.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Components/Mesh/StaticMesh.h"

using namespace NS_FbxTypeConverter;

std::shared_ptr<FFbxImporter> FFbxImporter::StaticInstance = nullptr;


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

bool FFbxImporter::ImportFromFile(const FString& InFilePath, UStaticMeshTest* OutStaticMesh, USkeletalMesh* OutSkeletalMesh)
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
		if (NodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			FbxMesh* Mesh = InNode->GetMesh();
			if (Mesh)
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
	OutMesh->AssetName = InNode->GetName();

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
	OutMesh->AssetName = InNode->GetName();

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
void FFbxImporter::ExtractMaterialData(const FbxNode* InNode, FbxMesh* InMesh, TArray<FMaterialInfo>& OutMaterials, TArray<FMeshSubset>& OutSubsets, const TArray<uint32>& InIndices) const
{
	const int MaterialCount = InNode->GetMaterialCount();
	OutMaterials.Empty(MaterialCount);
	OutSubsets.Empty(); // 서브셋은 동적으로 생성될 수 있음

	// 1. 머티리얼 정보 파싱
	for (int i = 0; i < MaterialCount; ++i)
	{
		FbxSurfaceMaterial* SurfaceMaterial = InNode->GetMaterial(i);
		if (!SurfaceMaterial) continue;

		FMaterialInfo NewMaterialInfo;
		NewMaterialInfo.Name = SurfaceMaterial->GetName();

		// PBR Standard Material 파싱 시도 (예시)
		if (SurfaceMaterial->GetClassId().Is(FbxSurfacePhong::ClassId)) // 또는 Lambert 등
		{
			// Diffuse Texture
			FbxProperty DiffuseProp = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);
			if (DiffuseProp.IsValid() && DiffuseProp.GetSrcObjectCount<FbxFileTexture>() > 0)
			{
				const FbxFileTexture* Texture = DiffuseProp.GetSrcObject<FbxFileTexture>(0);
				if (Texture) NewMaterialInfo.DiffuseMapPath = FString(Texture->GetFileName());
			}
			// Diffuse Color Factor
			FbxDouble3 DiffuseColor = static_cast<FbxSurfacePhong*>(SurfaceMaterial)->Diffuse.Get();
			NewMaterialInfo.DiffuseAlbedo = Convert<FLinearColor>(DiffuseColor);

			// TODO: Normal Map, Roughness, Metallic 등 파싱 (FBX 표준 속성 또는 커스텀 속성 확인 필요)
			// 예: Normal Map
            FbxProperty NormalProp = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sNormalMap);
             if (NormalProp.IsValid() && NormalProp.GetSrcObjectCount<FbxFileTexture>() > 0)
            {
                const FbxFileTexture* Texture = NormalProp.GetSrcObject<FbxFileTexture>(0);
                if (Texture) NewMaterialInfo.NormalMapPath = FString(Texture->GetFileName());
            }
		}
		// TODO: 다른 종류의 Surface Material 처리 (StandardPBR 등)

		OutMaterials.Add(NewMaterialInfo);
	}
	if(MaterialCount == 0)
	{
		// 기본 머티리얼 추가 (만약 필요하다면)
		FMaterialInfo DefaultMaterial;
		DefaultMaterial.Name = FName("DefaultMaterial");
		OutMaterials.Add(DefaultMaterial);
	}


	// 2. 서브셋 정보 생성 (머티리얼 할당 기반)
	FbxLayerElementMaterial* LayerMaterial = InMesh->GetLayer(0)->GetMaterials();
	if (!LayerMaterial)
	{
		// 머티리얼 할당 정보 없음 -> 단일 서브셋 사용 가정
		if (OutMaterials.Num() > 0)
		{
			FMeshSubset Subset;
			Subset.MaterialIndex = 0; // 첫 번째 (또는 기본) 머티리얼
			Subset.StartIndexLocation = 0;
			Subset.IndexCount = InIndices.Num();
			Subset.BaseVertexLocation = 0;
			OutSubsets.Add(Subset);
		}
		return;
	}

	const FbxLayerElement::EMappingMode MappingMode = LayerMaterial->GetMappingMode();
	const FbxLayerElement::EReferenceMode ReferenceMode = LayerMaterial->GetReferenceMode();

	if (MappingMode == FbxLayerElement::eAllSame)
	{
		// 모든 폴리곤이 같은 머티리얼 사용
		int MaterialIndex = 0;
		if (ReferenceMode == FbxLayerElement::eIndex || ReferenceMode == FbxLayerElement::eIndexToDirect)
		{
			if (LayerMaterial->GetIndexArray().GetCount() > 0)
				MaterialIndex = LayerMaterial->GetIndexArray().GetAt(0);
		}
		// 유효한 인덱스인지 확인
		if (MaterialIndex < 0 || MaterialIndex >= OutMaterials.Num()) MaterialIndex = 0;

		if (OutMaterials.Num() > 0)
		{
			FMeshSubset Subset;
			Subset.MaterialIndex = MaterialIndex;
			Subset.StartIndexLocation = 0;
			Subset.IndexCount = InIndices.Num();
			Subset.BaseVertexLocation = 0;
			OutSubsets.Add(Subset);
		}
	}
	else if (MappingMode == FbxLayerElement::eByPolygon)
	{
		// 폴리곤별로 머티리얼 인덱스 할당
		TMap<int32, TArray<uint32>> MaterialToIndicesMap; // MaterialIndex -> 해당 폴리곤들의 인덱스 목록
		const int PolygonCount = InMesh->GetPolygonCount();

		for (int PolyIdx = 0; PolyIdx < PolygonCount; ++PolyIdx)
		{
			int MaterialIndex = 0; // 기본값
			if (ReferenceMode == FbxLayerElement::eIndex || ReferenceMode == FbxLayerElement::eIndexToDirect)
			{
				if (PolyIdx < LayerMaterial->GetIndexArray().GetCount())
					MaterialIndex = LayerMaterial->GetIndexArray().GetAt(PolyIdx);
			}
			// 유효한 인덱스 보장
			if (MaterialIndex < 0 || MaterialIndex >= OutMaterials.Num()) MaterialIndex = 0;
			if (OutMaterials.IsEmpty()) MaterialIndex = -1; // 머티리얼 없으면 -1

			// 이 폴리곤(삼각형)에 해당하는 인덱스 3개를 가져옴
			const int32 StartIdx = PolyIdx * 3;
			TArray<uint32>& IndicesForMaterial = MaterialToIndicesMap.FindOrAdd(MaterialIndex);
			IndicesForMaterial.Add(InIndices[StartIdx]);
			IndicesForMaterial.Add(InIndices[StartIdx + 1]);
			IndicesForMaterial.Add(InIndices[StartIdx + 2]);
		}

		// 재구성된 인덱스 버퍼 및 서브셋 생성
		TArray<uint32> ReorderedIndices;
		uint32 CurrentStartIndex = 0;
		for (auto const& Pair : MaterialToIndicesMap)
		{
			const int32 MaterialIndex = Pair.Key;
			const TArray<uint32>& Indices = Pair.Value;

			if (MaterialIndex != -1 && !Indices.IsEmpty())
			{
				FMeshSubset Subset;
				Subset.MaterialIndex = MaterialIndex;
				Subset.StartIndexLocation = CurrentStartIndex;
				Subset.IndexCount = Indices.Num();
				Subset.BaseVertexLocation = 0; // BaseVertexLocation은 보통 0 (모든 버텍스를 하나의 버퍼에 넣는 경우)
				OutSubsets.Add(Subset);

				ReorderedIndices.Append(Indices);
				CurrentStartIndex += Indices.Num();
			}
		}
		// 원본 인덱스 배열을 재정렬된 것으로 교체 (주의: 이렇게 하면 원본 InIndices 파라미터는 변경되지 않음. 포인터나 레퍼런스로 받아야 변경 가능)
		// InIndices = ReorderedIndices; // 실제로는 이렇게 할당 X, OutIndices를 수정해야 함.
		// --> 이 함수는 InIndices를 const TArray<uint32>& 로 받는 것이 더 안전.
		// --> 또는 외부에서 인덱스 버퍼를 최종적으로 구성하도록 로직 변경 필요.
		// 여기서는 서브셋 정보만 생성하는 것으로 가정. 실제 인덱스 버퍼 구성은 별도 단계에서.
	} else {
         // UE_LOG(LogFbx, Warning, TEXT("Unsupported material mapping mode: %d"), MappingMode);
         // 지원하지 않는 모드 -> 기본 처리 (단일 서브셋 등)
         if (OutMaterials.Num() > 0) {
             FMeshSubset Subset;
             Subset.MaterialIndex = 0;
             Subset.StartIndexLocation = 0;
             Subset.IndexCount = InIndices.Num();
             Subset.BaseVertexLocation = 0;
             OutSubsets.Add(Subset);
         }
    }
	OutSubsets.Shrink();
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
