#include "FFbxLoader.h"

#include "UObject/ObjectFactory.h"
#include "Components/Material/Material.h"
#include "Components/Mesh/SkeletalMeshRenderData.h"

#include "Asset/SkeletalMeshAsset.h"

#include <fstream>
#include <sstream>

FbxManager* FFbxLoader::FbxManager = nullptr;
FbxIOSettings* FFbxLoader::IoSettings = nullptr;

bool FFbxLoader::ParseFBX(const FString& FilePath, FSkeletalMeshRenderData& OutRenderData)
{
    // Initialize FBX SDK
    if (!FbxManager)
    {
        FbxManager = FbxManager::Create();
    }
    if (!IoSettings)
    {
        IoSettings = FbxIOSettings::Create(FbxManager, IOSROOT);
        FbxManager->SetIOSettings(IoSettings);
    }
    // 비어있는 Scene을 생성
    FbxScene* Scene = FbxScene::Create(FbxManager, "Scene");
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
    FbxImporter* Importer = FbxImporter::Create(FbxManager, "");

    // Initialize the importer by providing a filename.
    const bool lImportStatus = Importer->Initialize(*FilePath, -1, FbxManager->GetIOSettings());
    Importer->GetFileVersion(lFileMajor, lFileMinor, lFileRevision);

    if (!lImportStatus)
    {
        FbxString error = Importer->GetStatus().GetErrorString();
        FBXSDK_printf("Call to FbxImporter::Initialize() failed.\n");
        FBXSDK_printf("Error returned: %s\n\n", error.Buffer());

        if (Importer->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion)
        {
            FBXSDK_printf("FBX file format version for this FBX SDK is %d.%d.%d\n", lSDKMajor, lSDKMinor, lSDKRevision);
            FBXSDK_printf("FBX file format version for file '%s' is %d.%d.%d\n\n", *FilePath, lFileMajor, lFileMinor, lFileRevision);
        }

        return false;
    }

    // Import를 해서 Scene에 저장하고 Importer를 파괴
    Importer->Import(Scene);
    Importer->Destroy();

    // Parse the FBX scene
    FbxNode* RootNode = Scene->GetRootNode();
    if (RootNode)
    {
        for (int i = 0; i < RootNode->GetChildCount(); ++i)
        {
            FbxNode* ChildNode = RootNode->GetChild(i);
            if (ChildNode->GetMesh())
            {
                ParseMesh(ChildNode->GetMesh(), OutRenderData);
            }
            ParseSkeleton(RootNode, OutRenderData);
        }
    }

    FbxManager->Destroy();
    return true;
}

void FFbxLoader::ParseMesh(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData)
{
    int ControlPointCount = Mesh->GetControlPointsCount();
    for (int i = 0; i < ControlPointCount; ++i)
    {
        FSkeletalMeshVertex Vertex;
        FbxVector4 Position = Mesh->GetControlPointAt(i);
        Vertex.Position = FVector(Position[0], Position[1], Position[2]);

        // Add default values for other attributes
        Vertex.Normal = FVector::ZeroVector;
        Vertex.Tangent = FVector::ZeroVector;
        Vertex.UV = FVector2D(0.0f, 0.0f);
        for (int j = 0; j < 4; ++j)
        {
            Vertex.BoneIndices[j] = -1;
            Vertex.BoneWeights[j] = 0.0f;
        }
        OutRenderData.Vertices.Add(Vertex);
    }

    // Parse Normals
    for (int i = 0; i < Mesh->GetPolygonCount(); ++i)
    {
        for (int j = 0; j < Mesh->GetPolygonSize(i); ++j)
        {
            int ControlPointIndex = Mesh->GetPolygonVertex(i, j);
            FbxVector4 Normal;
            Mesh->GetPolygonVertexNormal(i, j, Normal);
            OutRenderData.Vertices[ControlPointIndex].Normal = FVector(Normal[0], Normal[1], Normal[2]);
        }
    }

    // Parse UVs
    FbxStringList UVSetNameList;
    Mesh->GetUVSetNames(UVSetNameList);
    for (int i = 0; i < UVSetNameList.GetCount(); ++i)
    {
        const char* UVSetName = UVSetNameList.GetStringAt(i);
        for (int j = 0; j < Mesh->GetPolygonCount(); ++j)
        {
            for (int k = 0; k < Mesh->GetPolygonSize(j); ++k)
            {
                int ControlPointIndex = Mesh->GetPolygonVertex(j, k);
                FbxVector2 UV;
                bool Unmapped;
                Mesh->GetPolygonVertexUV(j, k, UVSetName, UV, Unmapped);
                OutRenderData.Vertices[ControlPointIndex].UV = FVector2D(UV[0], UV[1]);
            }
        }
    }

    // Parse Indices
    for (int i = 0; i < Mesh->GetPolygonCount(); ++i)
    {
        for (int j = 0; j < Mesh->GetPolygonSize(i); ++j)
        {
            OutRenderData.Indices.Add(Mesh->GetPolygonVertex(i, j));
        }
    }
}

void FFbxLoader::ParseSkeleton(FbxNode* RootNode, FSkeletalMeshRenderData& OutRenderData)
{
    for (int i = 0; i < RootNode->GetChildCount(); ++i)
    {
        FbxNode* ChildNode = RootNode->GetChild(i);
        if (ChildNode->GetSkeleton())
        {
            FSkeletalMeshBone Bone;
            Bone.Name = (ChildNode->GetName());
            Bone.ParentIndex = -1; // Handle parent relationship later
            Bone.BindPoseMatrix = FMatrix::Identity; // Replace with actual matrix
            OutRenderData.Bones.Add(Bone);
        }
        ParseSkeleton(ChildNode, OutRenderData);
    }
}

void FFbxLoader::ParseSkinningData(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData)
{
    FbxSkin* Skin = (FbxSkin*)Mesh->GetDeformer(0, FbxDeformer::eSkin);
    if (!Skin)
        return;

    for (int i = 0; i < Skin->GetClusterCount(); ++i)
    {
        FbxCluster* Cluster = Skin->GetCluster(i);
        FbxNode* LinkedBone = Cluster->GetLink();

        int BoneIndex = -1;
        for (int j = 0; j < OutRenderData.Bones.Num(); ++j)
        {
            if (OutRenderData.Bones[j].Name == (LinkedBone->GetName()))
            {
                BoneIndex = j;
                break;
            }
        }

        int* ControlPointIndices = Cluster->GetControlPointIndices();
        double* Weights = Cluster->GetControlPointWeights();
        for (int j = 0; j < Cluster->GetControlPointIndicesCount(); ++j)
        {
            int ControlPointIndex = ControlPointIndices[j];
            float Weight = static_cast<float>(Weights[j]);

            for (int k = 0; k < 4; ++k)
            {
                if (OutRenderData.Vertices[ControlPointIndex].BoneWeights[k] == 0.0f)
                {
                    OutRenderData.Vertices[ControlPointIndex].BoneIndices[k] = BoneIndex;
                    OutRenderData.Vertices[ControlPointIndex].BoneWeights[k] = Weight;
                    break;
                }
            }
        }
    }
}

FSkeletalMeshRenderData* FFbxManager::LoadFbxSkeletalMeshAsset(const FString& PathFileName)
{
    FSkeletalMeshRenderData* NewSkeletalMesh = new FSkeletalMeshRenderData();

    if (const auto It = FbxSkeletalMeshMap.Find(PathFileName))
    {
        return *It;
    }

    FWString BinaryPath = (PathFileName + ".bin").ToWideString();
    if (std::ifstream(BinaryPath).good())
    {
        if (LoadSkeletalMeshFromBinary(BinaryPath, *NewSkeletalMesh))
        {
            FbxSkeletalMeshMap.Add(PathFileName, NewSkeletalMesh);
            return NewSkeletalMesh;
        }
    }

    // Parse FBX
    FbxManager* Manager = FbxManager::Create();
    FbxIOSettings* IoSettings = FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(IoSettings);

    FbxImporter* Importer = FbxImporter::Create(Manager, "");
    if (!Importer->Initialize(*PathFileName, -1, Manager->GetIOSettings()))
    {
        UE_LOG(ELogLevel::Error, TEXT("Failed to initialize FBX Importer for file: %s"), *PathFileName);
        delete NewSkeletalMesh;
        return nullptr;
    }

    FbxScene* Scene = FbxScene::Create(Manager, "Scene");
    Importer->Import(Scene);
    Importer->Destroy();

    FbxNode* RootNode = Scene->GetRootNode();
    if (RootNode)
    {
        ParseFbxNode(RootNode, *NewSkeletalMesh);
    }
    else
    {
        UE_LOG(ELogLevel::Error, TEXT("FBX File has no root node: %s"), *PathFileName);
        delete NewSkeletalMesh;
        return nullptr;
    }

    Manager->Destroy();

    SaveSkeletalMeshToBinary(BinaryPath, *NewSkeletalMesh);
    FbxSkeletalMeshMap.Add(PathFileName, NewSkeletalMesh);
    return NewSkeletalMesh;
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


bool FFbxManager::SaveSkeletalMeshToBinary(const FWString& FilePath, const FSkeletalMeshRenderData& SkeletalMesh)
{
    std::ofstream File(FilePath, std::ios::binary);
    if (!File.is_open())
    {
        assert("CAN'T SAVE SKELETAL MESH BINARY FILE");
        return false;
    }

    // Object Name
    Serializer::WriteFWString(File, SkeletalMesh.ObjectName);

    // Display Name
    Serializer::WriteFString(File, SkeletalMesh.DisplayName);

    // Vertices
    uint32 VertexCount = SkeletalMesh.Vertices.Num();
    File.write(reinterpret_cast<const char*>(&VertexCount), sizeof(VertexCount));
    File.write(reinterpret_cast<const char*>(SkeletalMesh.Vertices.GetData()), VertexCount * sizeof(FSkeletalMeshVertex));

    // Indices
    uint32 IndexCount = SkeletalMesh.Indices.Num();
    File.write(reinterpret_cast<const char*>(&IndexCount), sizeof(IndexCount));
    File.write(reinterpret_cast<const char*>(SkeletalMesh.Indices.GetData()), IndexCount * sizeof(uint32));

    // Bones
    uint32 BoneCount = SkeletalMesh.Bones.Num();
    File.write(reinterpret_cast<const char*>(&BoneCount), sizeof(BoneCount));
    for (const FSkeletalMeshBone& Bone : SkeletalMesh.Bones)
    {
        Serializer::WriteFString(File, Bone.Name);
        File.write(reinterpret_cast<const char*>(&Bone.ParentIndex), sizeof(Bone.ParentIndex));
        File.write(reinterpret_cast<const char*>(&Bone.BindPoseMatrix), sizeof(FMatrix));
    }

    // Inverse Bind Pose Matrices
    uint32 MatrixCount = SkeletalMesh.InverseBindPoseMatrices.Num();
    File.write(reinterpret_cast<const char*>(&MatrixCount), sizeof(MatrixCount));
    File.write(reinterpret_cast<const char*>(SkeletalMesh.InverseBindPoseMatrices.GetData()), MatrixCount * sizeof(FMatrix));

    // Materials
    uint32 MaterialCount = SkeletalMesh.Materials.Num();
    File.write(reinterpret_cast<const char*>(&MaterialCount), sizeof(MaterialCount));
    for (const FMaterialInfo& Material : SkeletalMesh.Materials)
    {
        Serializer::WriteFString(File, Material.MaterialName);
        File.write(reinterpret_cast<const char*>(&Material.TextureFlag), sizeof(Material.TextureFlag));
        File.write(reinterpret_cast<const char*>(&Material.bTransparent), sizeof(Material.bTransparent));
        File.write(reinterpret_cast<const char*>(&Material.DiffuseColor), sizeof(Material.DiffuseColor));
        File.write(reinterpret_cast<const char*>(&Material.SpecularColor), sizeof(Material.SpecularColor));
        File.write(reinterpret_cast<const char*>(&Material.AmbientColor), sizeof(Material.AmbientColor));
        File.write(reinterpret_cast<const char*>(&Material.EmissiveColor), sizeof(Material.EmissiveColor));
        File.write(reinterpret_cast<const char*>(&Material.SpecularExponent), sizeof(Material.SpecularExponent));
        File.write(reinterpret_cast<const char*>(&Material.IOR), sizeof(Material.IOR));
        File.write(reinterpret_cast<const char*>(&Material.Transparency), sizeof(Material.Transparency));
        File.write(reinterpret_cast<const char*>(&Material.BumpMultiplier), sizeof(Material.BumpMultiplier));
        File.write(reinterpret_cast<const char*>(&Material.Metallic), sizeof(Material.Metallic));
        File.write(reinterpret_cast<const char*>(&Material.Roughness), sizeof(Material.Roughness));
    }

    File.close();
    return true;
}

bool FFbxManager::LoadSkeletalMeshFromBinary(const FWString& FilePath, FSkeletalMeshRenderData& OutSkeletalMesh)
{
    std::ifstream File(FilePath, std::ios::binary);
    if (!File.is_open())
    {
        assert("CAN'T OPEN SKELETAL MESH BINARY FILE");
        return false;
    }

    // Object Name
    Serializer::ReadFWString(File, OutSkeletalMesh.ObjectName);

    // Display Name
    Serializer::ReadFString(File, OutSkeletalMesh.DisplayName);

    // Vertices
    uint32 VertexCount = 0;
    File.read(reinterpret_cast<char*>(&VertexCount), sizeof(VertexCount));
    OutSkeletalMesh.Vertices.SetNum(VertexCount);
    File.read(reinterpret_cast<char*>(OutSkeletalMesh.Vertices.GetData()), VertexCount * sizeof(FSkeletalMeshVertex));

    // Indices
    uint32 IndexCount = 0;
    File.read(reinterpret_cast<char*>(&IndexCount), sizeof(IndexCount));
    OutSkeletalMesh.Indices.SetNum(IndexCount);
    File.read(reinterpret_cast<char*>(OutSkeletalMesh.Indices.GetData()), IndexCount * sizeof(uint32));

    // Bones
    uint32 BoneCount = 0;
    File.read(reinterpret_cast<char*>(&BoneCount), sizeof(BoneCount));
    OutSkeletalMesh.Bones.SetNum(BoneCount);
    for (FSkeletalMeshBone& Bone : OutSkeletalMesh.Bones)
    {
        Serializer::ReadFString(File, Bone.Name);
        File.read(reinterpret_cast<char*>(&Bone.ParentIndex), sizeof(Bone.ParentIndex));
        File.read(reinterpret_cast<char*>(&Bone.BindPoseMatrix), sizeof(FMatrix));
    }

    // Inverse Bind Pose Matrices
    uint32 MatrixCount = 0;
    File.read(reinterpret_cast<char*>(&MatrixCount), sizeof(MatrixCount));
    OutSkeletalMesh.InverseBindPoseMatrices.SetNum(MatrixCount);
    File.read(reinterpret_cast<char*>(OutSkeletalMesh.InverseBindPoseMatrices.GetData()), MatrixCount * sizeof(FMatrix));

    // Materials
    uint32 MaterialCount = 0;
    File.read(reinterpret_cast<char*>(&MaterialCount), sizeof(MaterialCount));
    OutSkeletalMesh.Materials.SetNum(MaterialCount);
    for (FMaterialInfo& Material : OutSkeletalMesh.Materials)
    {
        Serializer::ReadFString(File, Material.MaterialName);
        File.read(reinterpret_cast<char*>(&Material.TextureFlag), sizeof(Material.TextureFlag));
        File.read(reinterpret_cast<char*>(&Material.bTransparent), sizeof(Material.bTransparent));
        File.read(reinterpret_cast<char*>(&Material.DiffuseColor), sizeof(Material.DiffuseColor));
        File.read(reinterpret_cast<char*>(&Material.SpecularColor), sizeof(Material.SpecularColor));
        File.read(reinterpret_cast<char*>(&Material.AmbientColor), sizeof(Material.AmbientColor));
        File.read(reinterpret_cast<char*>(&Material.EmissiveColor), sizeof(Material.EmissiveColor));
        File.read(reinterpret_cast<char*>(&Material.SpecularExponent), sizeof(Material.SpecularExponent));
        File.read(reinterpret_cast<char*>(&Material.IOR), sizeof(Material.IOR));
        File.read(reinterpret_cast<char*>(&Material.Transparency), sizeof(Material.Transparency));
        File.read(reinterpret_cast<char*>(&Material.BumpMultiplier), sizeof(Material.BumpMultiplier));
        File.read(reinterpret_cast<char*>(&Material.Metallic), sizeof(Material.Metallic));
        File.read(reinterpret_cast<char*>(&Material.Roughness), sizeof(Material.Roughness));
    }

    File.close();
    return true;
}

void FFbxManager::ParseFbxNode(FbxNode* Node, FSkeletalMeshRenderData& OutRenderData)
{
    if (Node->GetMesh())
    {
        ParseMesh(Node->GetMesh(), OutRenderData);
    }

    for (int i = 0; i < Node->GetChildCount(); ++i)
    {
        ParseFbxNode(Node->GetChild(i), OutRenderData);
    }
}
void FFbxManager::ParseMesh(FbxMesh* Mesh, FSkeletalMeshRenderData& OutRenderData)
{
    // Parse Vertices
    int ControlPointCount = Mesh->GetControlPointsCount();
    OutRenderData.Vertices.SetNum(ControlPointCount);

    for (int i = 0; i < ControlPointCount; ++i)
    {
        FbxVector4 Position = Mesh->GetControlPointAt(i);
        OutRenderData.Vertices[i].Position = FVector(Position[0], Position[1], Position[2]);
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
                OutRenderData.Vertices[ControlPointIndex].Normal = FVector(Normal[0], Normal[1], Normal[2]);
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
                OutRenderData.Vertices[ControlPointIndex].Tangent = Tangent;
            }
        }
    }

    // Parse UVs
    FbxStringList UVSetNames;
    Mesh->GetUVSetNames(UVSetNames);
    for (int i = 0; i < UVSetNames.GetCount(); ++i)
    {
        const char* UVSetName = UVSetNames.GetStringAt(i);
        for (int j = 0; j < Mesh->GetPolygonCount(); ++j)
        {
            for (int k = 0; k < Mesh->GetPolygonSize(j); ++k)
            {
                int ControlPointIndex = Mesh->GetPolygonVertex(j, k);
                FbxVector2 UV;
                bool Unmapped;
                Mesh->GetPolygonVertexUV(j, k, UVSetName, UV, Unmapped);
                OutRenderData.Vertices[ControlPointIndex].UV = FVector2D(UV[0], 1.0f - UV[1]); // Flip V axis
            }
        }
    }

    // Parse Skinning Data (Bone Indices and Weights)
    FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(0, FbxDeformer::eSkin));
    if (Skin)
    {
        for (int ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ++ClusterIndex)
        {
            FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
            FbxNode* BoneNode = Cluster->GetLink();
            int BoneIndex = OutRenderData.Bones.Add({ BoneNode->GetName(), -1, FMatrix::Identity }); // Add to bone hierarchy

            // Set the parent bone index if applicable
            if (BoneNode->GetParent())
            {
                FString ParentName = BoneNode->GetParent()->GetName();
                for (int i = 0; i < OutRenderData.Bones.Num(); ++i)
                {
                    if (OutRenderData.Bones[i].Name == ParentName)
                    {
                        OutRenderData.Bones[BoneIndex].ParentIndex = i;
                        break;
                    }
                }
            }

            // Parse bone weights
            int* ControlPointIndices = Cluster->GetControlPointIndices();
            double* Weights = Cluster->GetControlPointWeights();
            for (int i = 0; i < Cluster->GetControlPointIndicesCount(); ++i)
            {
                int ControlPointIndex = ControlPointIndices[i];
                float Weight = static_cast<float>(Weights[i]);

                // Add bone influence to vertex
                for (int j = 0; j < 4; ++j)
                {
                    if (OutRenderData.Vertices[ControlPointIndex].BoneWeights[j] == 0.0f)
                    {
                        OutRenderData.Vertices[ControlPointIndex].BoneIndices[j] = BoneIndex;
                        OutRenderData.Vertices[ControlPointIndex].BoneWeights[j] = Weight;
                        break;
                    }
                }
            }
        }
    }

    // Parse Indices
    for (int i = 0; i < Mesh->GetPolygonCount(); ++i)
    {
        for (int j = 0; j < Mesh->GetPolygonSize(i); ++j)
        {
            OutRenderData.Indices.Add(Mesh->GetPolygonVertex(i, j));
        }
    }

    // Parse Materials
    for (int i = 0; i < Mesh->GetElementMaterialCount(); ++i)
    {
        fbxsdk::FbxSurfaceMaterial* Material = Mesh->GetNode()->GetMaterial(i);
        if (Material)
        {
            FMaterialInfo MaterialInfo;
            ParseMaterial(Material, MaterialInfo);
            OutRenderData.Materials.Add(MaterialInfo);
        }
    }
}

void FFbxManager::ParseMaterial(FbxSurfaceMaterial* Material, FMaterialInfo& OutMaterialInfo)
{
    OutMaterialInfo.MaterialName = Material->GetName();

    //// Diffuse
    //FbxProperty DiffuseProperty = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
    //if (DiffuseProperty.IsValid())
    //{
    //    FbxDouble3 Diffuse = DiffuseProperty.Get<FbxDouble3>();
    //    OutMaterialInfo.DiffuseColor = FVector(Diffuse[0], Diffuse[1], Diffuse[2]);
    //}

    //// Specular
    //FbxProperty SpecularProperty = Material->FindProperty(FbxSurfaceMaterial::sSpecular);
    //if (SpecularProperty.IsValid())
    //{
    //    FbxDouble3 Specular = SpecularProperty.Get<FbxDouble3>();
    //    OutMaterialInfo.SpecularColor = FVector(Specular[0], Specular[1], Specular[2]);
    //}

    //// Emissive
    //FbxProperty EmissiveProperty = Material->FindProperty(FbxSurfaceMaterial::sEmissive);
    //if (EmissiveProperty.IsValid())
    //{
    //    FbxDouble3 Emissive = EmissiveProperty.Get<FbxDouble3>();
    //    OutMaterialInfo.EmissiveColor = FVector(Emissive[0], Emissive[1], Emissive[2]);
    //}

//    // Transparency
//    FbxProperty TransparencyProperty = Material->FindProperty(fbxsdk::FbxSurfaceMaterial::sTransparencyFactor);
//    if (TransparencyProperty.IsValid())
//    {
//        OutMaterialInfo.Transparency = static_cast<float>(TransparencyProperty.Get<FbxDouble>());
//    }
}

USkeletalMesh* FFbxManager::CreateSkeletalMesh(const FString& filePath)
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
