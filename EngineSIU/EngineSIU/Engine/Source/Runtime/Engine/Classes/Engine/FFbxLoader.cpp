#include "FFbxLoader.h"

#include "UObject/ObjectFactory.h"
#include "Components/Material/Material.h"
#include "Components/Mesh/SkeletalMeshRenderData.h"

#include "Asset/SkeletalMeshAsset.h"

#include <fstream>
#include <sstream>

FbxManager* FFbxLoader::SDKManager = nullptr;
FbxIOSettings* FFbxLoader::IoSettings = nullptr;

bool FFbxLoader::ParseFBX(const FWString& FilePath, FSkeletalMeshRenderData& OutRenderData)
{
    // Initialize FBX SDK
    if (!SDKManager)
    {
        SDKManager = FbxManager::Create();
    }
    if (!IoSettings)
    {
        IoSettings = FbxIOSettings::Create(SDKManager, IOSROOT);
        SDKManager->SetIOSettings(IoSettings);
    }
    // 비어있는 Scene을 생성
    FbxScene* Scene = FbxScene::Create(SDKManager, "Scene");
    if (!Scene)
    {
        FBXSDK_printf("Error: Unable to create FBX scene!\n");
        exit(1);
    }

    int lFileMajor, lFileMinor, lFileRevision;
    int lSDKMajor, lSDKMinor, lSDKRevision;
    //int lFileFormat = -1;
    int lAnimStackCount;
    bool lStatus;
    char lPassword[1024];

    // Get the file version number generate by the FBX SDK.
    FbxManager::GetFileFormatVersion(lSDKMajor, lSDKMinor, lSDKRevision);

    // Create an importer.
    FbxImporter* Importer = FbxImporter::Create(SDKManager, "");

    // Initialize the importer by providing a filename.
    FString FilePathChar = WStringToString(FilePath);
    const bool lImportStatus = Importer->Initialize(*FilePathChar, -1, SDKManager->GetIOSettings());
    Importer->GetFileVersion(lFileMajor, lFileMinor, lFileRevision);

    if (!lImportStatus)
    {
        FbxString error = Importer->GetStatus().GetErrorString();
        FBXSDK_printf("Call to FbxImporter::Initialize() failed.\n");
        FBXSDK_printf("Error returned: %s\n\n", error.Buffer());

        if (Importer->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion)
        {
            FBXSDK_printf("FBX file format version for this FBX SDK is %d.%d.%d\n", lSDKMajor, lSDKMinor, lSDKRevision);
            FBXSDK_printf("FBX file format version for file '%s' is %d.%d.%d\n\n", FilePath.c_str(), lFileMajor, lFileMinor, lFileRevision);
        }

        return false;
    }

    // Import를 해서 Scene에 저장하고 Importer를 파괴
    Importer->Import(Scene);
    Importer->Destroy();

    // polygon으로 변경
    FbxGeometryConverter GeomConverter(SDKManager);
    try {
        GeomConverter.Triangulate(Scene, true);
    }
    catch (std::runtime_error) {
        FBXSDK_printf("Scene integrity verification failed.\n");
        return false;
    }

    OutRenderData.ObjectName = FilePath.c_str();

    // texture부터 로드
    LoadTexture(Scene, FilePath);

    // scene으로부터 로드
    LoadSceneRecursive(Scene, OutRenderData);

    return true;
}

// FBX파일에 연결되어 있는 texture를 ResourceManager에 등록
void FFbxLoader::LoadTexture(FbxScene* Scene, const FWString FilePath)
{
    const int TextureCount = Scene->GetTextureCount();
    for (int TextureIndex = 0; TextureIndex < TextureCount; ++TextureIndex)
    {
        FbxTexture* Texture = Scene->GetTexture(TextureIndex);
        fbxsdk::FbxFileTexture* FileTexture = FbxCast<fbxsdk::FbxFileTexture>(Texture);
        if (FileTexture/* && FileTexture->GetUserDataPtr()*/)
        {
            // !!! 접근할 때에는 항싱 FbxTexture::GetFileName()으로 가져와야 함.
            const FbxString TextureFileName = FileTexture->GetFileName(); // 절대경로

            const char* FileNameChar = TextureFileName.Buffer();
            const FWString TexturePath(FileNameChar, &FileNameChar[TextureFileName.GetLen()]);
            // 이미 저장한 texture인지 확인
            if (FEngineLoop::ResourceManager.GetTexture(TexturePath))
            {
                continue;
            }
            // 없으면 새로 생성
            else if (S_OK == FEngineLoop::ResourceManager.LoadTextureFromFile(FEngineLoop::GraphicDevice.Device, TexturePath.c_str(), true)) // FBX는 항상 srgb 사용
            {
                continue;
            }
        }
    }
}

void FFbxLoader::LoadSceneRecursive(FbxScene* Scene, FSkeletalMeshRenderData& OutRenderData)
{
    // Get the root node of the scene
     FbxNode* RootNode = Scene->GetRootNode();
    if (RootNode)
    {
        LoadNodeRecursive(RootNode, OutRenderData);
    }
}

void FFbxLoader::LoadNodeRecursive(FbxNode* Node, FSkeletalMeshRenderData& OutRenderData)
{
    //if (FbxNodeAttribute* Attribute = Node->GetNodeAttribute())
    //{
    //    // Check if the node has a mesh attribute
    //    if(Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            // Parse the node's mesh
            if (Node->GetMesh())
            {
                FbxMesh* Mesh = Node->GetMesh();
                uint32 VertexBase = OutRenderData.Vertices.Num();
                ParseMesh(Mesh, OutRenderData, VertexBase); // Vertex별 파싱
                ParseSkinningData(Mesh, OutRenderData, VertexBase); // Skeleton 관련된 파싱
                ParseMeshByMaterial(Node, OutRenderData); // Material별 Vertex 지정
            }
        }
        // Parse the node's mesh
        // Recursively parse child nodes
        for (int i = 0; i < Node->GetChildCount(); ++i)
        {
            LoadNodeRecursive(Node->GetChild(i), OutRenderData);
        }

    //}

    // 현재 최대 64개만 받음
    if (OutRenderData.Bones.Num() > 64)
    {
            OutRenderData.bCPUSkinning = true;
    }
}

/// <summary>
/// FbxSurfaceMaterial을 파싱하여 FMaterialInfo에 저장합니다.
/// OBJ 전용인 필드는 비워놓습니다.
/// 이전 단계(LoadTexture)에서 Texture는 모두 저장되었다고 가정하고, Texture는 이름만 저장합니다.
/// </summary>
/// <param name="Material"></param>
/// <param name="OutMaterialInfo"></param>
void FFbxLoader::ParseMaterial(FbxSurfaceMaterial* Material, FMaterialInfo& OutMaterialInfo)
{
    // FMaterialInfo 순서대로 파싱
    OutMaterialInfo.MaterialName = Material->GetName();

    // Diffuse
    FbxProperty DiffuseProperty = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
    if (DiffuseProperty.IsValid())
    {
        // Diffuse 값을 파싱
        FbxDouble3 Diffuse = DiffuseProperty.Get<FbxDouble3>();
        OutMaterialInfo.DiffuseColor = FVector(Diffuse[0], Diffuse[1], Diffuse[2]);

        // Diffuse Texture 파싱
        int TextureCount = DiffuseProperty.GetSrcObjectCount<FbxFileTexture>();
        for (int Count = 0; Count < TextureCount; ++Count)
        {
            FbxFileTexture* Texture = DiffuseProperty.GetSrcObject<FbxFileTexture>(Count);
            if (Texture)
            {
                const FbxString TextureFileName = Texture->GetFileName();
                const char* FileNameChar = TextureFileName.Buffer();
                const FWString TexturePath(FileNameChar, &FileNameChar[TextureFileName.GetLen()]);

                FTextureInfo TextureInfo;
                TextureInfo.TextureName = Texture->GetName();
                TextureInfo.TexturePath = TexturePath;
                TextureInfo.bIsSRGB = true; // FBX는 항상 srgb 사용
                OutMaterialInfo.TextureInfos.Add(TextureInfo);
            }
        }
    }

    // Specular
    FbxProperty SpecularProperty = Material->FindProperty(FbxSurfaceMaterial::sSpecular);
    if (SpecularProperty.IsValid())
    {
        FbxDouble3 Specular = SpecularProperty.Get<FbxDouble3>();
        OutMaterialInfo.SpecularColor = FVector(Specular[0], Specular[1], Specular[2]);

        // Specular Texture 파싱
        int TextureCount = SpecularProperty.GetSrcObjectCount<FbxFileTexture>();
        for (int Count = 0; Count < TextureCount; ++Count)
        {
            FbxFileTexture* Texture = SpecularProperty.GetSrcObject<FbxFileTexture>(Count);
            if (Texture)
            {
                const FbxString TextureFileName = Texture->GetFileName();
                const char* FileNameChar = TextureFileName.Buffer();
                const FWString TexturePath(FileNameChar, &FileNameChar[TextureFileName.GetLen()]);
                FTextureInfo TextureInfo;
                TextureInfo.TextureName = Texture->GetName();
                TextureInfo.TexturePath = TexturePath;
                TextureInfo.bIsSRGB = true; // FBX는 항상 srgb 사용
                OutMaterialInfo.TextureInfos.Add(TextureInfo);
            }
        }
    }

    // Ambient
    FbxProperty AmbientProperty = Material->FindProperty(FbxSurfaceMaterial::sAmbient);
    if (AmbientProperty.IsValid())
    {
        FbxDouble3 Ambient = AmbientProperty.Get<FbxDouble3>();
        OutMaterialInfo.AmbientColor = FVector(Ambient[0], Ambient[1], Ambient[2]);
        // Ambient Texture 파싱
        int TextureCount = AmbientProperty.GetSrcObjectCount<FbxFileTexture>();
        for (int Count = 0; Count < TextureCount; ++Count)
        {
            FbxFileTexture* Texture = AmbientProperty.GetSrcObject<FbxFileTexture>(Count);
            if (Texture)
            {
                const FbxString TextureFileName = Texture->GetFileName();
                const char* FileNameChar = TextureFileName.Buffer();
                const FWString TexturePath(FileNameChar, &FileNameChar[TextureFileName.GetLen()]);
                FTextureInfo TextureInfo;
                TextureInfo.TextureName = Texture->GetName();
                TextureInfo.TexturePath = TexturePath;
                TextureInfo.bIsSRGB = true; // FBX는 항상 srgb 사용
                OutMaterialInfo.TextureInfos.Add(TextureInfo);
            }
        }
    }

    // Emissive
    FbxProperty EmissiveProperty = Material->FindProperty(FbxSurfaceMaterial::sEmissive);
    if (EmissiveProperty.IsValid())
    {
        FbxDouble3 Emissive = EmissiveProperty.Get<FbxDouble3>();
        OutMaterialInfo.EmissiveColor = FVector(Emissive[0], Emissive[1], Emissive[2]);
        // Emissive Texture 파싱
        int TextureCount = EmissiveProperty.GetSrcObjectCount<FbxFileTexture>();
        for (int Count = 0; Count < TextureCount; ++Count)
        {
            FbxFileTexture* Texture = EmissiveProperty.GetSrcObject<FbxFileTexture>(Count);
            if (Texture)
            {
                const FbxString TextureFileName = Texture->GetFileName();
                const char* FileNameChar = TextureFileName.Buffer();
                const FWString TexturePath(FileNameChar, &FileNameChar[TextureFileName.GetLen()]);
                FTextureInfo TextureInfo;
                TextureInfo.TextureName = Texture->GetName();
                TextureInfo.TexturePath = TexturePath;
                TextureInfo.bIsSRGB = true; // FBX는 항상 srgb 사용
                OutMaterialInfo.TextureInfos.Add(TextureInfo);
            }
        }
    }

    // Shininess
    FbxProperty ShininessProperty = Material->FindProperty(FbxSurfaceMaterial::sShininess);
    if (ShininessProperty.IsValid())
    {
        OutMaterialInfo.SpecularExponent = static_cast<float>(ShininessProperty.Get<FbxDouble>());
    }

    // Transparency
    FbxProperty TransparencyProperty = Material->FindProperty(fbxsdk::FbxSurfaceMaterial::sTransparencyFactor);
    if (TransparencyProperty.IsValid())
    {
        OutMaterialInfo.Transparency = static_cast<float>(TransparencyProperty.Get<FbxDouble>());
        if (OutMaterialInfo.Transparency > 0.0f)
        {
            OutMaterialInfo.bTransparent = true;
        }
    }

    // Bump
    FbxProperty BumpProperty = Material->FindProperty(FbxSurfaceMaterial::sBump);
    if (BumpProperty.IsValid())
    {
        FbxDouble3 Bump = BumpProperty.Get<FbxDouble3>();
        OutMaterialInfo.BumpMultiplier = static_cast<float>(Bump[0]);
        // Bump Texture 파싱
        int TextureCount = BumpProperty.GetSrcObjectCount<FbxFileTexture>();
        for (int Count = 0; Count < TextureCount; ++Count)
        {
            FbxFileTexture* Texture = BumpProperty.GetSrcObject<FbxFileTexture>(Count);
            if (Texture)
            {
                const FbxString TextureFileName = Texture->GetFileName();
                const char* FileNameChar = TextureFileName.Buffer();
                const FWString TexturePath(FileNameChar, &FileNameChar[TextureFileName.GetLen()]);
                FTextureInfo TextureInfo;
                TextureInfo.TextureName = Texture->GetName();
                TextureInfo.TexturePath = TexturePath;
                TextureInfo.bIsSRGB = true; // FBX는 항상 srgb 사용
                OutMaterialInfo.TextureInfos.Add(TextureInfo);
            }
        }
    }
}

void FFbxLoader::ParseSkeleton(FbxNode* Node, FSkeletalMeshRenderData& OutRenderData, uint32 VertexBase)
{
}

/// Vertex별 정보를 파싱합니다.
void FFbxLoader::ParseMesh(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData, uint32 VertexBase)
{
    // Parse Vertices
    int ControlPointCount = Mesh->GetControlPointsCount(); // 전체 버텍스 개수

    for (int i = 0; i < ControlPointCount; ++i)
    {
        FbxVector4 Position = Mesh->GetControlPointAt(i);
        FSkeletalMeshVertex Vertex;
        Vertex.Position = FVector(Position[0], Position[1], Position[2]);
        OutRenderData.Vertices.Add(Vertex);
    }

    // Parse Normals
    FbxGeometryElementNormal* NormalElement = Mesh->GetElementNormal();
    if (NormalElement)
    {
        for (int i = 0; i < Mesh->GetPolygonCount(); ++i)
        {
            for (int j = 0; j < Mesh->GetPolygonSize(i); ++j)
            {
                int ControlPointIndex = Mesh->GetPolygonVertex(i, j);
                FbxVector4 Normal;
                Mesh->GetPolygonVertexNormal(i, j, Normal);
                OutRenderData.Vertices[VertexBase + ControlPointIndex].Normal = FVector(Normal[0], Normal[1], Normal[2]);
            }
        }
    }

    // Parse Tangents
    FbxGeometryElementTangent* TangentElement = Mesh->GetElementTangent();
    if (TangentElement)
    {
        for (int PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
        {
            for (int VertIndex = 0; VertIndex < Mesh->GetPolygonSize(PolygonIndex); ++VertIndex)
            {
                int ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, VertIndex);

                // Get Tangent depending on the mapping mode
                FVector Tangent = FVector::ZeroVector;
                if (TangentElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
                {
                    int TangentIndex = TangentElement->GetIndexArray().GetAt(PolygonIndex * Mesh->GetPolygonSize(PolygonIndex) + VertIndex);
                    FbxVector4 FbxTangent = TangentElement->GetDirectArray().GetAt(TangentIndex);
                    Tangent = FVector(FbxTangent[0], FbxTangent[1], FbxTangent[2]);
                }
                else if (TangentElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
                {
                    int TangentIndex = ControlPointIndex;
                    FbxVector4 FbxTangent = TangentElement->GetDirectArray().GetAt(TangentIndex);
                    Tangent = FVector(FbxTangent[0], FbxTangent[1], FbxTangent[2]);
                }

                // Store the tangent in your render data
                OutRenderData.Vertices[VertexBase + ControlPointIndex].Tangent = Tangent;
            }
        }
    }

    //// Parse UVs
    //FbxStringList UVSetNames;
    //Mesh->GetUVSetNames(UVSetNames);
    //for (int i = 0; i < UVSetNames.GetCount(); ++i)
    //{
    //    const char* UVSetName = UVSetNames.GetStringAt(i);
    //    for (int j = 0; j < Mesh->GetPolygonCount(); ++j)
    //    {
    //        for (int k = 0; k < Mesh->GetPolygonSize(j); ++k)
    //        {
    //            int ControlPointIndex = Mesh->GetPolygonVertex(j, k);
    //            FbxVector2 UV;
    //            bool Unmapped;
    //            Mesh->GetPolygonVertexUV(j, k, UVSetName, UV, Unmapped);
    //            // Unmap되어있어도 할게없음...
    //            OutRenderData.Vertices[VertexBase + ControlPointIndex].UV = FVector2D(UV[0], 1.0f - UV[1]); // Flip V axis
    //            OutRenderData.Vertices[VertexBase + ControlPointIndex].MaterialIndex = Mesh->GetMaterialIndices()[j];
    //        }
    //    }
    //}

    // Parse Indices
    for (int i = 0; i < Mesh->GetPolygonCount(); ++i)
    {
        for (int j = 0; j < Mesh->GetPolygonSize(i); ++j)
        {
            OutRenderData.Indices.Add(Mesh->GetPolygonVertex(i, j));
        }
    }
}

void FFbxLoader::ParseMeshByMaterial(FbxNode* Node, FSkeletalMeshRenderData& OutRenderData)
{
    // Parse the material
    const int MaterialCount = Node->GetMaterialCount();
    bool bNoMaterial = (MaterialCount == 0);

    // 1. 머티리얼 없는 경우: 기본 머티리얼에 연결
    if (bNoMaterial)
    {
        static const FString DefaultMaterialName = TEXT("DefaultMaterialName_FFbxLoader");
        bool bDefaultMaterialDefined = false;
        // 기본 머터리얼이 정의되어 있는지 확인
        for (const FMaterialInfo& Mat : OutRenderData.Materials)
        {
            if (Mat.MaterialName == DefaultMaterialName)
            {
                bDefaultMaterialDefined = true;
                break;
            }
        }

        // 기본 머터리얼이 없을 경우 추가
        if (!bDefaultMaterialDefined)
        {
            FMaterialInfo DefaultMaterial;
            DefaultMaterial.MaterialName = DefaultMaterialName;
            // TODO : FFbxLoader가 잘 작동하는 것을 확인하면 
            // 로드되지 않은 머터리얼이 잘보이게 빨간색으로 바꿀 필요 있음.
            DefaultMaterial.DiffuseColor = FVector(0.7f, 0.7f, 0.7f);
            DefaultMaterial.SpecularColor = FVector(0.2f, 0.2f, 0.2f);
            DefaultMaterial.EmissiveColor = FVector(0, 0, 0);
            DefaultMaterial.Transparency = 0.0f;
            OutRenderData.Materials.Add(DefaultMaterial);
        }

        // Material Subset도 기본값 추가 (메쉬 전체를 기본 머티리얼로)
        FMaterialSubset DefaultSubset;
        DefaultSubset.IndexStart = 0;
        DefaultSubset.IndexCount = OutRenderData.Indices.Num();
        DefaultSubset.MaterialIndex = 0;
        DefaultSubset.MaterialName = DefaultMaterialName;
        OutRenderData.MaterialSubsets.Add(DefaultSubset);
    }
    else
    {
        // 머티리얼 파싱
        for (int MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
        {
            FbxSurfaceMaterial* Material = Node->GetMaterial(MaterialIndex);
            if (Material)
            {
                FMaterialInfo MaterialInfo;
                ParseMaterial(Material, MaterialInfo);
                OutRenderData.Materials.Add(MaterialInfo);
            }
        }

        FbxMesh* Mesh = Node->GetMesh();
        int PolygonCount = Mesh->GetPolygonCount();
        FbxLayerElementMaterial* MaterialLayer = Mesh->GetElementMaterial();

        TMap<int32, TArray<int32>> MaterialToIndexBuffer;

        // 2. Material별 IndexBuffer 분리
        // !!!
        // Control Point Vertex : 위치 정보만을 가진 버텍스
        // Polygon Vertex : Control Point Vertex + UV, Normal, Tangent 등
        // 저장해야 할 것은 Polygon Vertex
        switch (MaterialLayer->GetMappingMode())
        {
        case FbxGeometryElement::eByPolygon:
        {
            for (int PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
            {
                int MaterialIndex = MaterialLayer->GetIndexArray().GetAt(PolygonIndex);
                for (int VertexIndex = 0; VertexIndex < Mesh->GetPolygonSize(PolygonIndex); ++VertexIndex)
                {
                    int ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, VertexIndex);
                    // TODO: 반드시 Unique Vertex Buffer 사용
                    MaterialToIndexBuffer.FindOrAdd(MaterialIndex).Add(ControlPointIndex);
                }
            }
            break;
        }
        case FbxGeometryElement::eByControlPoint:
        {
            for (int PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
            {
                for (int VertexIndex = 0; VertexIndex < Mesh->GetPolygonSize(PolygonIndex); ++VertexIndex)
                {
                    int ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, VertexIndex);
                    int MaterialIndex = MaterialLayer->GetIndexArray().GetAt(ControlPointIndex);
                    // TODO: 반드시 Unique Vertex Buffer 사용
                    MaterialToIndexBuffer.FindOrAdd(MaterialIndex).Add(ControlPointIndex);
                }
            }
            break;
        }
        case FbxGeometryElement::eByPolygonVertex:
        {
            int VertexCounter = 0;
            for (int PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
            {
                for (int VertexIndex = 0; VertexIndex < Mesh->GetPolygonSize(PolygonIndex); ++VertexIndex, ++VertexCounter)
                {
                    int ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, VertexIndex);
                    int MaterialIndex = MaterialLayer->GetIndexArray().GetAt(VertexCounter);
                    // TODO: 반드시 Unique Vertex Buffer 사용
                    MaterialToIndexBuffer.FindOrAdd(MaterialIndex).Add(ControlPointIndex);
                }
            }
            break;
        }
        case FbxGeometryElement::eAllSame:
        {
            int MaterialIndex = MaterialLayer->GetIndexArray().GetAt(0);
            for (int PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
            {
                for (int VertexIndex = 0; VertexIndex < Mesh->GetPolygonSize(PolygonIndex); ++VertexIndex)
                {
                    int ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, VertexIndex);
                    // TODO: 반드시 Unique Vertex Buffer 사용
                    MaterialToIndexBuffer.FindOrAdd(MaterialIndex).Add(ControlPointIndex);
                }
            }
            break;
        }
        default:
            // 다른 매핑 방식은 필요에 따라 구현
            break;
        }

        // 이후 머티리얼 서브셋 생성 (이전 코드와 동일)
        int IndexStart = 0;
        for (auto& Pair : MaterialToIndexBuffer)
        {
            int MatIndex = Pair.Key;
            const TArray<int32>& Indices = Pair.Value;

            FMaterialSubset NewSubset;
            NewSubset.MaterialIndex = MatIndex;
            NewSubset.IndexStart = IndexStart;
            NewSubset.IndexCount = Indices.Num();
            NewSubset.MaterialName = OutRenderData.Materials.IsValidIndex(MatIndex)
                ? OutRenderData.Materials[MatIndex].MaterialName
                : FString::Printf(TEXT("Material_%d"), MatIndex);

            OutRenderData.Indices + (Indices);
            OutRenderData.MaterialSubsets.Add(NewSubset);

            IndexStart += Indices.Num();
        }
    }
}

// Bone과 Skinning Data(Vertex별 Bone Index, Weights...)를 파싱합니다.
void FFbxLoader::ParseSkinningData(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData, uint32 VertexBase)
{
    // OutRenderData의 기존 정보를 간섭하지 않도록 주의
    FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(0, FbxDeformer::eSkin));
    // Skin이 있으면 해당 Skin을 이용
    if (Skin)
    {
        int ClusterCount = Skin->GetClusterCount(); // Bone 개수

        // ClusterCount만큼 Matrix를 추가
        int NumMatrices = OutRenderData.InverseBindPoseMatrices.Num();
        //OutRenderData.InverseBindPoseMatrices.SetNum(NumMatrices + ClusterCount);
        //OutRenderData.Bones.SetNum(NumMatrices + ClusterCount);

        for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
        { 
            FbxCluster* Cluster = Skin->GetCluster(ClusterIndex); // Bone
            FbxNode* BoneNode = Cluster->GetLink(); // Bone이 속한 Node

            // Add Bone to FSkeletalMeshBone
            FSkeletalMeshBone Bone;
            Bone.Name = BoneNode->GetName();
            Bone.ParentIndex = -1; // 최상위 index

            // Set Parent Bone Index
            if (BoneNode->GetParent())
            {
                FString ParentName = BoneNode->GetParent()->GetName();
                for (int i = 0; i < OutRenderData.Bones.Num(); ++i)
                {
                    if (OutRenderData.Bones[i].Name == ParentName)
                    {
                        Bone.ParentIndex = i;
                        break;
                    }
                }
            }

            // Parse Bind Pose Matrix
            FbxAMatrix BindPoseMatrix;
            Cluster->GetTransformLinkMatrix(BindPoseMatrix); // 부모까지 
            FbxAMatrix GlobalBindPoseMatrix;
            Cluster->GetTransformMatrix(GlobalBindPoseMatrix); // 나까지 포함됨

            FMatrix ConvertedMatrix = {
                BindPoseMatrix.Get(0, 0), BindPoseMatrix.Get(0,1), BindPoseMatrix.Get(0, 2), BindPoseMatrix.Get(0, 3),
                BindPoseMatrix.Get(1, 0), BindPoseMatrix.Get(1, 1), BindPoseMatrix.Get(1, 2), BindPoseMatrix.Get(1, 3),
                BindPoseMatrix.Get(2, 0), BindPoseMatrix.Get(2, 1), BindPoseMatrix.Get(2, 2), BindPoseMatrix.Get(2, 3),
                BindPoseMatrix.Get(3, 0), BindPoseMatrix.Get(3, 1), BindPoseMatrix.Get(3, 2), BindPoseMatrix.Get(3, 3)
            };

            FMatrix ConvertedGlobalMatrix = {
                GlobalBindPoseMatrix.Get(0, 0), GlobalBindPoseMatrix.Get(0, 1), GlobalBindPoseMatrix.Get(0, 2), GlobalBindPoseMatrix.Get(0, 3),
                GlobalBindPoseMatrix.Get(1, 0), GlobalBindPoseMatrix.Get(1, 1), GlobalBindPoseMatrix.Get(1, 2), GlobalBindPoseMatrix.Get(1, 3),
                GlobalBindPoseMatrix.Get(2, 0), GlobalBindPoseMatrix.Get(2, 1), GlobalBindPoseMatrix.Get(2, 2), GlobalBindPoseMatrix.Get(2, 3),
                GlobalBindPoseMatrix.Get(3, 0), GlobalBindPoseMatrix.Get(3, 1), GlobalBindPoseMatrix.Get(3, 2), GlobalBindPoseMatrix.Get(3, 3)
            };

            Bone.BindPoseMatrix = FMatrix::Inverse(ConvertedMatrix) * ConvertedGlobalMatrix;
            OutRenderData.Bones.Add(Bone);

            // Store the inverse bind pose matrix
            OutRenderData.InverseBindPoseMatrices.Add(FMatrix::Inverse(ConvertedMatrix));

            // Parse Bone Weights
            int* ControlPointIndices = Cluster->GetControlPointIndices();
            double* Weights = Cluster->GetControlPointWeights();
            int InfluenceCount = Cluster->GetControlPointIndicesCount();

            for (int i = 0; i < InfluenceCount; ++i)
            {
                int ControlPointIndex = ControlPointIndices[i];
                float Weight = static_cast<float>(Weights[i]);
                int GlobalIndex = VertexBase + ControlPointIndex;

                // Add bone influence to vertex
                for (int j = 0; j < 8; ++j)
                {
                    // 
                    // 비어있는 인덱스에 저장
                    if (OutRenderData.Vertices[GlobalIndex].BoneWeights[3] != 0.0f)
                    {
                        int asdf = 0;
                    }
                    if (OutRenderData.Vertices[GlobalIndex].BoneWeights[j] == 0.0f)
                    {
                        if (OutRenderData.Bones.Num() - 1 == 1 && GlobalIndex > 10000)
                        {
                            int asdf = 0;
                        }
                        OutRenderData.Vertices[GlobalIndex].BoneIndices[j] = OutRenderData.Bones.Num() - 1;
                        //assert(OutRenderData.Bones.Num() - 1 != 1 && GlobalIndex > 10000);
                        OutRenderData.Vertices[GlobalIndex].BoneWeights[j] = Weight;
                        break;
                    }
                }
            }
        }
    }
    // TODO : StaticMesh6로 넘기기
    // Bone이 없는 경우
    else
    {
        // No skinning data: treat as static mesh attached to single root bone
        // 1. Root bone 추가
        FSkeletalMeshBone RootBone;
        RootBone.Name = TEXT("Root");
        RootBone.ParentIndex = -1;
        RootBone.BindPoseMatrix = FMatrix::Identity;
        OutRenderData.Bones.Add(RootBone);

        // 2. Inverse Bind Pose Matrix
        int BoneIndex = OutRenderData.InverseBindPoseMatrices.Num();
        OutRenderData.InverseBindPoseMatrices.Add(FMatrix::Identity);

        // 3. 모든 버텍스에 Root Bone 100% 영향 할당
        for (int32 i = 0; i < OutRenderData.Vertices.Num(); ++i)
        {
            OutRenderData.Vertices[i].BoneIndices[0] = BoneIndex;
            OutRenderData.Vertices[i].BoneWeights[0] = 1.0f;
            for (int j = 1; j < 8; ++j)
            {
                OutRenderData.Vertices[i].BoneIndices[j] = 0;
                OutRenderData.Vertices[i].BoneWeights[j] = 0.0f;
            }
        }
    }
}

FSkeletalMeshRenderData* FFbxManager::LoadFbxSkeletalMeshAsset(const FWString& PathFileName)
{
    if (auto It = FbxSkeletalMeshMap.Find(PathFileName))
    {
        return It; // Fix: Dereference the iterator to get the actual value and return its address.  
    }

    // 일단은 제외
    //FWString BinaryPath = (PathFileName + ".bin").ToWideString();
    //if (std::ifstream(BinaryPath).good())
    //{
    //    if (LoadSkeletalMeshFromBinary(BinaryPath, *NewSkeletalMesh))
    //    {
    //        FbxSkeletalMeshMap.Add(PathFileName, NewSkeletalMesh);
    //        return NewSkeletalMesh;
    //    }
    //}

    ///

    FSkeletalMeshRenderData RenderData;
    if (FFbxLoader::ParseFBX(PathFileName, RenderData))
    {
        //SaveSkeletalMeshToBinary(BinaryPath, *NewSkeletalMesh);
        FbxSkeletalMeshMap.Add(PathFileName, RenderData);
        return &FbxSkeletalMeshMap[PathFileName];
    }
    return nullptr;

}

UMaterial* FFbxManager::CreateMaterial(const FMaterialInfo& MaterialInfo)
{
    // Check if the material already exists in the MaterialMap
    if (MaterialMap[MaterialInfo.MaterialName] != nullptr)
    {
        return MaterialMap[MaterialInfo.MaterialName];
    }

    // Create a new material using the FObjectFactory
    UMaterial* NewMaterial = FObjectFactory::ConstructObject<UMaterial>(nullptr);
    if (!NewMaterial)
    {
        return nullptr; // Return nullptr if material creation fails
    }

    // Set the material properties
    NewMaterial->SetMaterialInfo(MaterialInfo);

    // Add the new material to the MaterialMap
    MaterialMap.Add(MaterialInfo.MaterialName, NewMaterial);

    return NewMaterial;
}
//
//
//bool FFbxManager::SaveSkeletalMeshToBinary(const FWString& FilePath, const FSkeletalMeshRenderData& SkeletalMesh)
//{
//    std::ofstream File(FilePath, std::ios::binary);
//    if (!File.is_open())
//    {
//        assert("CAN'T SAVE SKELETAL MESH BINARY FILE");
//        return false;
//    }
//
//    // Object Name
//    Serializer::WriteFWString(File, SkeletalMesh.ObjectName);
//
//    // Display Name
//    Serializer::WriteFString(File, SkeletalMesh.DisplayName);
//
//    // Vertices
//    uint32 VertexCount = SkeletalMesh.Vertices.Num();
//    File.write(reinterpret_cast<const char*>(&VertexCount), sizeof(VertexCount));
//    File.write(reinterpret_cast<const char*>(SkeletalMesh.Vertices.GetData()), VertexCount * sizeof(FSkeletalMeshVertex));
//
//    // Indices
//    uint32 IndexCount = SkeletalMesh.Indices.Num();
//    File.write(reinterpret_cast<const char*>(&IndexCount), sizeof(IndexCount));
//    File.write(reinterpret_cast<const char*>(SkeletalMesh.Indices.GetData()), IndexCount * sizeof(uint32));
//
//    // Bones
//    uint32 BoneCount = SkeletalMesh.Bones.Num();
//    File.write(reinterpret_cast<const char*>(&BoneCount), sizeof(BoneCount));
//    for (const FSkeletalMeshBone& Bone : SkeletalMesh.Bones)
//    {
//        Serializer::WriteFString(File, Bone.Name);
//        File.write(reinterpret_cast<const char*>(&Bone.ParentIndex), sizeof(Bone.ParentIndex));
//        File.write(reinterpret_cast<const char*>(&Bone.BindPoseMatrix), sizeof(FMatrix));
//    }
//
//    // Inverse Bind Pose Matrices
//    uint32 MatrixCount = SkeletalMesh.InverseBindPoseMatrices.Num();
//    File.write(reinterpret_cast<const char*>(&MatrixCount), sizeof(MatrixCount));
//    File.write(reinterpret_cast<const char*>(SkeletalMesh.InverseBindPoseMatrices.GetData()), MatrixCount * sizeof(FMatrix));
//
//    // Materials
//    uint32 MaterialCount = SkeletalMesh.Materials.Num();
//    File.write(reinterpret_cast<const char*>(&MaterialCount), sizeof(MaterialCount));
//    for (const FMaterialInfo& Material : SkeletalMesh.Materials)
//    {
//        Serializer::WriteFString(File, Material.MaterialName);
//        File.write(reinterpret_cast<const char*>(&Material.TextureFlag), sizeof(Material.TextureFlag));
//        File.write(reinterpret_cast<const char*>(&Material.bTransparent), sizeof(Material.bTransparent));
//        File.write(reinterpret_cast<const char*>(&Material.DiffuseColor), sizeof(Material.DiffuseColor));
//        File.write(reinterpret_cast<const char*>(&Material.SpecularColor), sizeof(Material.SpecularColor));
//        File.write(reinterpret_cast<const char*>(&Material.AmbientColor), sizeof(Material.AmbientColor));
//        File.write(reinterpret_cast<const char*>(&Material.EmissiveColor), sizeof(Material.EmissiveColor));
//        File.write(reinterpret_cast<const char*>(&Material.SpecularExponent), sizeof(Material.SpecularExponent));
//        File.write(reinterpret_cast<const char*>(&Material.IOR), sizeof(Material.IOR));
//        File.write(reinterpret_cast<const char*>(&Material.Transparency), sizeof(Material.Transparency));
//        File.write(reinterpret_cast<const char*>(&Material.BumpMultiplier), sizeof(Material.BumpMultiplier));
//        File.write(reinterpret_cast<const char*>(&Material.Metallic), sizeof(Material.Metallic));
//        File.write(reinterpret_cast<const char*>(&Material.Roughness), sizeof(Material.Roughness));
//    }
//
//    File.close();
//    return true;
//}
//
//bool FFbxManager::LoadSkeletalMeshFromBinary(const FWString& FilePath, FSkeletalMeshRenderData& OutSkeletalMesh)
//{
//    std::ifstream File(FilePath, std::ios::binary);
//    if (!File.is_open())
//    {
//        assert("CAN'T OPEN SKELETAL MESH BINARY FILE");
//        return false;
//    }
//
//    // Object Name
//    Serializer::ReadFWString(File, OutSkeletalMesh.ObjectName);
//
//    // Display Name
//    Serializer::ReadFString(File, OutSkeletalMesh.DisplayName);
//
//    // Vertices
//    uint32 VertexCount = 0;
//    File.read(reinterpret_cast<char*>(&VertexCount), sizeof(VertexCount));
//    OutSkeletalMesh.Vertices.SetNum(VertexCount);
//    File.read(reinterpret_cast<char*>(OutSkeletalMesh.Vertices.GetData()), VertexCount * sizeof(FSkeletalMeshVertex));
//
//    // Indices
//    uint32 IndexCount = 0;
//    File.read(reinterpret_cast<char*>(&IndexCount), sizeof(IndexCount));
//    OutSkeletalMesh.Indices.SetNum(IndexCount);
//    File.read(reinterpret_cast<char*>(OutSkeletalMesh.Indices.GetData()), IndexCount * sizeof(uint32));
//
//    // Bones
//    uint32 BoneCount = 0;
//    File.read(reinterpret_cast<char*>(&BoneCount), sizeof(BoneCount));
//    OutSkeletalMesh.Bones.SetNum(BoneCount);
//    for (FSkeletalMeshBone& Bone : OutSkeletalMesh.Bones)
//    {
//        Serializer::ReadFString(File, Bone.Name);
//        File.read(reinterpret_cast<char*>(&Bone.ParentIndex), sizeof(Bone.ParentIndex));
//        File.read(reinterpret_cast<char*>(&Bone.BindPoseMatrix), sizeof(FMatrix));
//    }
//
//    // Inverse Bind Pose Matrices
//    uint32 MatrixCount = 0;
//    File.read(reinterpret_cast<char*>(&MatrixCount), sizeof(MatrixCount));
//    OutSkeletalMesh.InverseBindPoseMatrices.SetNum(MatrixCount);
//    File.read(reinterpret_cast<char*>(OutSkeletalMesh.InverseBindPoseMatrices.GetData()), MatrixCount * sizeof(FMatrix));
//
//    // Materials
//    uint32 MaterialCount = 0;
//    File.read(reinterpret_cast<char*>(&MaterialCount), sizeof(MaterialCount));
//    OutSkeletalMesh.Materials.SetNum(MaterialCount);
//    for (FMaterialInfo& Material : OutSkeletalMesh.Materials)
//    {
//        Serializer::ReadFString(File, Material.MaterialName);
//        File.read(reinterpret_cast<char*>(&Material.TextureFlag), sizeof(Material.TextureFlag));
//        File.read(reinterpret_cast<char*>(&Material.bTransparent), sizeof(Material.bTransparent));
//        File.read(reinterpret_cast<char*>(&Material.DiffuseColor), sizeof(Material.DiffuseColor));
//        File.read(reinterpret_cast<char*>(&Material.SpecularColor), sizeof(Material.SpecularColor));
//        File.read(reinterpret_cast<char*>(&Material.AmbientColor), sizeof(Material.AmbientColor));
//        File.read(reinterpret_cast<char*>(&Material.EmissiveColor), sizeof(Material.EmissiveColor));
//        File.read(reinterpret_cast<char*>(&Material.SpecularExponent), sizeof(Material.SpecularExponent));
//        File.read(reinterpret_cast<char*>(&Material.IOR), sizeof(Material.IOR));
//        File.read(reinterpret_cast<char*>(&Material.Transparency), sizeof(Material.Transparency));
//        File.read(reinterpret_cast<char*>(&Material.BumpMultiplier), sizeof(Material.BumpMultiplier));
//        File.read(reinterpret_cast<char*>(&Material.Metallic), sizeof(Material.Metallic));
//        File.read(reinterpret_cast<char*>(&Material.Roughness), sizeof(Material.Roughness));
//    }
//
//    File.close();
//    return true;
//}
//
//void FFbxManager::ParseFbxNode(FbxNode* Node, FSkeletalMeshRenderData& OutRenderData)
//{
//    if (Node->GetMesh())
//    {
//        ParseMesh(Node->GetMesh(), OutRenderData);
//    }
//
//    for (int i = 0; i < Node->GetChildCount(); ++i)
//    {
//        ParseFbxNode(Node->GetChild(i), OutRenderData);
//    }
//}
//void FFbxManager::ParseMesh(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData)
//{
//    // Parse Vertices
//    int ControlPointCount = Mesh->GetControlPointsCount();
//    OutRenderData.Vertices.SetNum(ControlPointCount);
//
//    for (int i = 0; i < ControlPointCount; ++i)
//    {
//        FbxVector4 Position = Mesh->GetControlPointAt(i);
//        OutRenderData.Vertices[i].Position = FVector(Position[0], Position[1], Position[2]);
//    }
//
//    // Parse Normals
//    FbxGeometryElementNormal* NormalElement = Mesh->GetElementNormal();
//    if (NormalElement)
//    {
//        for (int i = 0; i < Mesh->GetPolygonCount(); ++i)
//        {
//            for (int j = 0; j < Mesh->GetPolygonSize(i); ++j)
//            {
//                int ControlPointIndex = Mesh->GetPolygonVertex(i, j);
//                FbxVector4 Normal;
//                Mesh->GetPolygonVertexNormal(i, j, Normal);
//                OutRenderData.Vertices[ControlPointIndex].Normal = FVector(Normal[0], Normal[1], Normal[2]);
//            }
//        }
//    }
//
//    // Parse Tangents
//    FbxGeometryElementTangent* TangentElement = Mesh->GetElementTangent();
//    if (TangentElement)
//    {
//        for (int PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
//        {
//            for (int VertIndex = 0; VertIndex < Mesh->GetPolygonSize(PolygonIndex); ++VertIndex)
//            {
//                int ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, VertIndex);
//
//                // Get Tangent depending on the mapping mode
//                FVector Tangent = FVector::ZeroVector;
//                if (TangentElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
//                {
//                    int TangentIndex = TangentElement->GetIndexArray().GetAt(PolygonIndex * Mesh->GetPolygonSize(PolygonIndex) + VertIndex);
//                    FbxVector4 FbxTangent = TangentElement->GetDirectArray().GetAt(TangentIndex);
//                    Tangent = FVector(FbxTangent[0], FbxTangent[1], FbxTangent[2]);
//                }
//                else if (TangentElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
//                {
//                    int TangentIndex = ControlPointIndex;
//                    FbxVector4 FbxTangent = TangentElement->GetDirectArray().GetAt(TangentIndex);
//                    Tangent = FVector(FbxTangent[0], FbxTangent[1], FbxTangent[2]);
//                }
//
//                // Store the tangent in your render data
//                OutRenderData.Vertices[ControlPointIndex].Tangent = Tangent;
//            }
//        }
//    }
//
//    // Parse UVs
//    FbxStringList UVSetNames;
//    Mesh->GetUVSetNames(UVSetNames);
//    for (int i = 0; i < UVSetNames.GetCount(); ++i)
//    {
//        const char* UVSetName = UVSetNames.GetStringAt(i);
//        for (int j = 0; j < Mesh->GetPolygonCount(); ++j)
//        {
//            for (int k = 0; k < Mesh->GetPolygonSize(j); ++k)
//            {
//                int ControlPointIndex = Mesh->GetPolygonVertex(j, k);
//                FbxVector2 UV;
//                bool Unmapped;
//                Mesh->GetPolygonVertexUV(j, k, UVSetName, UV, Unmapped);
//                OutRenderData.Vertices[ControlPointIndex].UV = FVector2D(UV[0], 1.0f - UV[1]); // Flip V axis
//            }
//        }
//    }
//
//    // Parse Skinning Data (Bone Indices and Weights)
//    FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(0, FbxDeformer::eSkin));
//    if (Skin)
//    {
//        for (int ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ++ClusterIndex)
//        {
//            FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
//            FbxNode* BoneNode = Cluster->GetLink();
//            int BoneIndex = OutRenderData.Bones.Add({ BoneNode->GetName(), -1, FMatrix::Identity }); // Add to bone hierarchy
//
//            // Set the parent bone index if applicable
//            if (BoneNode->GetParent())
//            {
//                FString ParentName = BoneNode->GetParent()->GetName();
//                for (int i = 0; i < OutRenderData.Bones.Num(); ++i)
//                {
//                    if (OutRenderData.Bones[i].Name == ParentName)
//                    {
//                        OutRenderData.Bones[BoneIndex].ParentIndex = i;
//                        break;
//                    }
//                }
//            }
//
//            // Parse bone weights
//            int* ControlPointIndices = Cluster->GetControlPointIndices();
//            double* Weights = Cluster->GetControlPointWeights();
//            for (int i = 0; i < Cluster->GetControlPointIndicesCount(); ++i)
//            {
//                int ControlPointIndex = ControlPointIndices[i];
//                float Weight = static_cast<float>(Weights[i]);
//
//                // Add bone influence to vertex
//                for (int j = 0; j < 4; ++j)
//                {
//                    if (OutRenderData.Vertices[ControlPointIndex].BoneWeights[j] == 0.0f)
//                    {
//                        OutRenderData.Vertices[ControlPointIndex].BoneIndices[j] = BoneIndex;
//                        OutRenderData.Vertices[ControlPointIndex].BoneWeights[j] = Weight;
//                        break;
//                    }
//                }
//            }
//        }
//    }
//
//    // Parse Indices
//    for (int i = 0; i < Mesh->GetPolygonCount(); ++i)
//    {
//        for (int j = 0; j < Mesh->GetPolygonSize(i); ++j)
//        {
//            OutRenderData.Indices.Add(Mesh->GetPolygonVertex(i, j));
//        }
//    }
//
//    // Parse Materials
//    for (int i = 0; i < Mesh->GetElementMaterialCount(); ++i)
//    {
//        fbxsdk::FbxSurfaceMaterial* Material = Mesh->GetNode()->GetMaterial(i);
//        if (Material)
//        {
//            FMaterialInfo MaterialInfo;
//            ParseMaterial(Material, MaterialInfo);
//            OutRenderData.Materials.Add(MaterialInfo);
//        }
//    }
//}
//
//void FFbxManager::ParseMaterial(FbxSurfaceMaterial* Material, FMaterialInfo& OutMaterialInfo)
//{
//    OutMaterialInfo.MaterialName = Material->GetName();
//
//    //// Diffuse
//    //FbxProperty DiffuseProperty = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
//    //if (DiffuseProperty.IsValid())
//    //{
//    //    FbxDouble3 Diffuse = DiffuseProperty.Get<FbxDouble3>();
//    //    OutMaterialInfo.DiffuseColor = FVector(Diffuse[0], Diffuse[1], Diffuse[2]);
//    //}
//
//    //// Specular
//    //FbxProperty SpecularProperty = Material->FindProperty(FbxSurfaceMaterial::sSpecular);
//    //if (SpecularProperty.IsValid())
//    //{
//    //    FbxDouble3 Specular = SpecularProperty.Get<FbxDouble3>();
//    //    OutMaterialInfo.SpecularColor = FVector(Specular[0], Specular[1], Specular[2]);
//    //}
//
//    //// Emissive
//    //FbxProperty EmissiveProperty = Material->FindProperty(FbxSurfaceMaterial::sEmissive);
//    //if (EmissiveProperty.IsValid())
//    //{
//    //    FbxDouble3 Emissive = EmissiveProperty.Get<FbxDouble3>();
//    //    OutMaterialInfo.EmissiveColor = FVector(Emissive[0], Emissive[1], Emissive[2]);
//    //}
//
////    // Transparency
////    FbxProperty TransparencyProperty = Material->FindProperty(fbxsdk::FbxSurfaceMaterial::sTransparencyFactor);
////    if (TransparencyProperty.IsValid())
////    {
////        OutMaterialInfo.Transparency = static_cast<float>(TransparencyProperty.Get<FbxDouble>());
////    }
//}
//
USkeletalMesh* FFbxManager::CreateSkeletalMesh(const FWString& filePath)
{
    // Load Skeletal Mesh Render Data from file
    FSkeletalMeshRenderData* SkeletalMeshRenderData = FFbxManager::LoadFbxSkeletalMeshAsset(filePath);

    // If loading failed, return nullptr
    if (SkeletalMeshRenderData == nullptr) return nullptr;

    // Check if the Skeletal Mesh already exists in the map using the object's name
    USkeletalMesh* SkeletalMesh = GetSkeletalMesh(SkeletalMeshRenderData->ObjectName);
    if (SkeletalMesh != nullptr)
    {
        return SkeletalMesh;
    }

    // Construct a new Skeletal Mesh object
    SkeletalMesh = FObjectFactory::ConstructObject<USkeletalMesh>(nullptr); // TODO: Integrate with AssetManager for better resource handling.
    SkeletalMesh->SetData(SkeletalMeshRenderData);

    // Add the new Skeletal Mesh to the map for future reference
    SkeletalMeshMap.Add(SkeletalMeshRenderData->ObjectName, SkeletalMesh); // TODO: Consider using file paths as keys for better scalability.

    return SkeletalMesh;
}

USkeletalMesh* FFbxManager::GetSkeletalMesh(const FWString& ObjectName)
{
    // Check if the Skeletal Mesh exists in the map
    if (const auto It = SkeletalMeshMap.Find(ObjectName))
    {
        return *It;
    }
    return nullptr; // Return nullptr if not found
}
