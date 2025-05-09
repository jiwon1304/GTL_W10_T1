#include "FFbxLoader.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include "FbxObject.h"
#include "Serializer.h"
#include "UObject/ObjectFactory.h"
#include "Components/Material/Material.h"
#include "Engine/Asset/SkeletalMeshAsset.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Container/StringConv.h"

struct BoneWeights
{
    int jointIndex;
    float weight;
};

FFbxSkeletalMesh* FFbxLoader::ParseFBX(const FString& FBXFilePath)
{
    if (fbxMap.Contains(FBXFilePath))
        return fbxMap[FBXFilePath];
    
    FbxScene* scene = FbxScene::Create(FFbxLoader::GetFbxManager(), "");
    FbxImporter* importer = FbxImporter::Create(GetFbxManager(), "");
    
    if (!importer->Initialize(GetData(FBXFilePath), -1, GetFbxIOSettings()))
    {
        importer->Destroy();
        return nullptr;
    }

    if (importer->IsFBX())
    {
        FbxIOSettings* iosettings = GetFbxIOSettings();
        iosettings->SetBoolProp(IMP_FBX_MATERIAL, true);
        iosettings->SetBoolProp(IMP_FBX_TEXTURE, true);
        iosettings->SetBoolProp(IMP_FBX_LINK, true);
        iosettings->SetBoolProp(IMP_FBX_SHAPE, true);
        iosettings->SetBoolProp(IMP_FBX_GOBO, true);
        iosettings->SetBoolProp(IMP_FBX_ANIMATION, true);
        iosettings->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);
    }

    bool bIsImported = importer->Import(scene);
    importer->Destroy();
    if (!bIsImported)
    {
        return nullptr;   
    }

    // convert scene
    FbxAxisSystem sceneAxisSystem = scene->GetGlobalSettings().GetAxisSystem();
    FbxAxisSystem targetAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);
    if (sceneAxisSystem != targetAxisSystem)
    {
        targetAxisSystem.ConvertScene(scene);
    }
    
    FbxSystemUnit SceneSystemUnit = scene->GetGlobalSettings().GetSystemUnit();
    if( SceneSystemUnit.GetScaleFactor() != 1.0 )
    {
        FbxSystemUnit::cm.ConvertScene(scene);
    }
    
    FbxGeometryConverter converter(GetFbxManager());
    converter.Triangulate(scene, true);

    FFbxSkeletalMesh* result = LoadFBXObject(scene);
    scene->Destroy();
    result->name = FBXFilePath;
    fbxMap[FBXFilePath] = result;
    return result;
}

USkeletalMesh* FFbxLoader::GetFbxObject(const FString& filename)
{
    FWString BinaryPath = (filename + ".bin").ToWideString();

    // Last Modified Time
    auto FileTime = std::filesystem::last_write_time(filename.ToWideString());
    int64_t lastModifiedTime = std::chrono::system_clock::to_time_t(
    std::chrono::time_point_cast<std::chrono::system_clock::duration>(
    FileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()));
    
    // 미리 저장해놓은게 있으면 반환
    if (SkeletalMeshMap.Contains(filename))
    {
        return SkeletalMeshMap[filename];
    }
    
    // 없으면 파싱
    FFbxSkeletalMesh* fbxObject = new FFbxSkeletalMesh();
    bool bCreateNewMesh = true;
    
    if (std::ifstream(BinaryPath).good())
    {
        // bin
        if (FFbxManager::LoadFBXFromBinary(BinaryPath, lastModifiedTime, *fbxObject))
        {
            fbxMap[filename] = fbxObject;
            bCreateNewMesh = false;
        }
    }

    if (bCreateNewMesh)
    {
        fbxObject = GetFbxObjectInternal(filename);
        if (fbxObject)
        {
            FFbxManager::SaveFBXToBinary(BinaryPath, lastModifiedTime, *fbxObject);
        }
    }
    
    if (!fbxObject) // 파싱 실패
    {
        delete fbxObject;
        return nullptr;
    }

    // SkeletalMesh로 변환
    USkeletalMesh* newSkeletalMesh = FObjectFactory::ConstructObject<USkeletalMesh>(nullptr);

    FSkeletalMeshRenderData renderData;
    renderData.ObjectName = fbxObject->name;
    renderData.RenderSections.SetNum(fbxObject->mesh.Num());
    renderData.MaterialSubsets.SetNum(fbxObject->materialSubsets.Num());

    for (int i = 0; i < fbxObject->mesh.Num(); ++i)
    {
        // TArray로 직접 접근해도 돼나?
        // 두 구조체의 메모리 레이아웃이 같아야함.
        renderData.RenderSections[i].Vertices.SetNum(fbxObject->mesh[i].vertices.Num());
        for (int j = 0; j < fbxObject->mesh[i].vertices.Num(); ++j)
        {
            renderData.RenderSections[i].Vertices[j].Position = fbxObject->mesh[i].vertices[j].position;
            renderData.RenderSections[i].Vertices[j].Color = fbxObject->mesh[i].vertices[j].color;
            renderData.RenderSections[i].Vertices[j].Normal = fbxObject->mesh[i].vertices[j].normal;
            renderData.RenderSections[i].Vertices[j].Tangent = fbxObject->mesh[i].vertices[j].tangent;
            renderData.RenderSections[i].Vertices[j].UV = fbxObject->mesh[i].vertices[j].uv;
            renderData.RenderSections[i].Vertices[j].MaterialIndex = fbxObject->mesh[i].vertices[j].materialIndex;
            memcpy(
                renderData.RenderSections[i].Vertices[j].BoneIndices,
                fbxObject->mesh[i].vertices[j].boneIndices,
                sizeof(int8) * 8
            );
            memcpy(
                renderData.RenderSections[i].Vertices[j].BoneWeights,
                fbxObject->mesh[i].vertices[j].boneWeights,
                sizeof(float) * 8
            );
        }
        memcpy(
            renderData.RenderSections[i].Vertices.GetData(),
            fbxObject->mesh[i].vertices.GetData(),
            sizeof(FFbxVertex) * fbxObject->mesh[i].vertices.Num()
        );

        renderData.RenderSections[i].Indices = fbxObject->mesh[i].indices;
        renderData.RenderSections[i].SubsetIndex = fbxObject->mesh[i].subsetIndex;
        renderData.RenderSections[i].Name = fbxObject->mesh[i].name;
    }
    for (int i = 0; i < fbxObject->materialSubsets.Num(); ++i)
    {
        renderData.MaterialSubsets[i] = fbxObject->materialSubsets[i];
    }
    //RenderDatas.Add(renderData);

    FReferenceSkeleton refSkeleton;
    refSkeleton.RawRefBoneInfo.SetNum(fbxObject->skeleton.joints.Num());
    refSkeleton.RawRefBonePose.SetNum(fbxObject->skeleton.joints.Num());
    //refSkeleton.RawNameToIndexMap.Reserve(fbxObject->skeleton.joints.Num());
    for (int i = 0; i < fbxObject->skeleton.joints.Num(); ++i)
    {
        refSkeleton.RawRefBoneInfo[i].Name = fbxObject->skeleton.joints[i].name;
        refSkeleton.RawRefBoneInfo[i].ParentIndex = fbxObject->skeleton.joints[i].parentIndex;
        refSkeleton.RawRefBonePose[i].Translation = fbxObject->skeleton.joints[i].position;
        refSkeleton.RawRefBonePose[i].Rotation = fbxObject->skeleton.joints[i].rotation;
        refSkeleton.RawRefBonePose[i].Scale3D = fbxObject->skeleton.joints[i].scale;
        refSkeleton.RawNameToIndexMap.Add(fbxObject->skeleton.joints[i].name, i);
    }

    TArray<UMaterial*> Materials = fbxObject->material;

    //// 추가된 요소의 포인터 얻기
    //FSkeletalMeshRenderData* pRenderData = &RenderDatas[RenderDatas.Num()-1];

    TArray<FMatrix> InverseBindPoseMatrices;
    InverseBindPoseMatrices.SetNum(fbxObject->skeleton.joints.Num());
    for (int i = 0; i < fbxObject->skeleton.joints.Num(); ++i)
    {
        InverseBindPoseMatrices[i] = fbxObject->skeleton.joints[i].inverseBindPose;
    }
    newSkeletalMesh->SetData(renderData, refSkeleton, InverseBindPoseMatrices, Materials);
    newSkeletalMesh->bCPUSkinned = InverseBindPoseMatrices.Num() > 128 ? 1 : 0; // GPU Skinning 최대 bone 개수 128개를 넘어가면 CPU로 전환
    SkeletalMeshMap.Add(filename, newSkeletalMesh);
    return newSkeletalMesh;
}

FFbxSkeletalMesh* FFbxLoader::GetFbxObjectInternal(const FString& filename)
{
    if (!fbxMap.Contains(filename))
        ParseFBX(filename);
    return fbxMap[filename];
}

FFbxSkeletalMesh* FFbxLoader::LoadFBXObject(FbxScene* InFbxInfo)
{
    FFbxSkeletalMesh* result = new FFbxSkeletalMesh();

    TArray<TMap<int, TArray<BoneWeights>>> weightMaps;
    TMap<FString, int> boneNameToIndex;

    TArray<FbxNode*> skeletons;
    TArray<FbxNode*> meshes;
    
    std::function<void(FbxNode*)> Traverse = [&](FbxNode* Node)
    {   if (!Node) return;
        FbxNodeAttribute* attr = Node->GetNodeAttribute();
        if (attr)
        {
            switch (attr->GetAttributeType())
            {
            case FbxNodeAttribute::eSkeleton:
                skeletons.Add(Node);
                break;
            case FbxNodeAttribute::eMesh:
                meshes.Add(Node);
                break;
            default:
                break;
            }
        }
        for (int i = 0; i < Node->GetChildCount(); ++i)
        {
            Traverse(Node->GetChild(i));
        }
    };
    
    Traverse(InFbxInfo->GetRootNode());

    // parse bones
    for (auto& node : skeletons)
    {
        LoadFbxSkeleton(result, node, boneNameToIndex, -1);
    }

    // parse skins
    for (int i = 0; i < meshes.Num(); ++i)
    {
        FbxNode*& node = meshes[i];
        TMap<int, TArray<BoneWeights>> weightMap;
        LoadSkinWeights(node, boneNameToIndex, weightMap);
        weightMaps.Add(weightMap);
    }

    // parse meshes & material
    for (int i = 0; i < meshes.Num(); ++i)
    {
        FbxNode*& node = meshes[i];
        LoadFBXMesh(result, node, boneNameToIndex, weightMaps[i]);
        LoadFBXMaterials(result, node);
    }

    return result;
}

FbxManager* FFbxLoader::GetFbxManager()
{
    static FbxManager* fbxManager = FbxManager::Create();
    return fbxManager;
}

FbxIOSettings* FFbxLoader::GetFbxIOSettings()
{
    if (GetFbxManager()->GetIOSettings() == nullptr)
    {
        GetFbxManager()->SetIOSettings(FbxIOSettings::Create(GetFbxManager(), "IOSRoot"));
    }
    return GetFbxManager()->GetIOSettings();
}

FbxCluster* FFbxLoader::FindClusterForBone(FbxNode* boneNode)
{
    if (!boneNode || !boneNode->GetScene()) return nullptr;
    FbxScene* scene = boneNode->GetScene();

    for (int i = 0; i < scene->GetRootNode()->GetChildCount(); ++i)
    {
        FbxNode* meshNode = scene->GetRootNode()->GetChild(i);
        FbxMesh* mesh = meshNode ? meshNode->GetMesh() : nullptr;
        if (!mesh) continue;

        int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
        for (int s = 0; s < skinCount; ++s)
        {
            FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(s, FbxDeformer::eSkin));
            for (int c = 0; c < skin->GetClusterCount(); ++c)
            {
                FbxCluster* cluster = skin->GetCluster(c);
                if (cluster->GetLink() == boneNode)
                    return cluster;
            }
        }
    }
    return nullptr;
}

void FFbxLoader::LoadFbxSkeleton(
    FFbxSkeletalMesh* fbxObject,
    FbxNode* node,
    TMap<FString, int>& boneNameToIndex,
    int parentIndex = -1
)
{
    if (!node || boneNameToIndex.Contains(node->GetName()))
        return;

    FbxNodeAttribute* attr = node->GetNodeAttribute();
    if (!attr || attr->GetAttributeType() != FbxNodeAttribute::eSkeleton)
    {
        for (int i = 0; i < node->GetChildCount(); ++i)
        {
            LoadFbxSkeleton(fbxObject, node->GetChild(i), boneNameToIndex, parentIndex);
        }
        return;
    }

    FFbxJoint joint;
    joint.name = node->GetName();
    joint.parentIndex = parentIndex;

    FbxCluster* cluster = FindClusterForBone(node);
    FbxPose* pose = nullptr;
    for (int i = 0; i < node->GetScene()->GetPoseCount(); ++i)
    {
        pose = node->GetScene()->GetPose(i);
        if (pose->IsBindPose())
            break;
    }
    
    // https://blog.naver.com/jidon333/220264383892
    // Mesh -> Deformer -> Cluster -> Link == "joint"
    // bone은 joint사이의 공간을 말하는거지만, 사실상 joint와 동일한 의미로 사용되고 있음.
    if (cluster)
    {
        FbxAMatrix LinkMatrix, Matrix;
        cluster->GetTransformLinkMatrix(LinkMatrix);  // !!! 실제 joint Matrix : joint->model space 변환 행렬
        cluster->GetTransformMatrix(Matrix);      // Fbx 모델의 전역 오프셋 : 모든 joint가 같은 값을 가짐
        FbxAMatrix InverseMatrix = LinkMatrix.Inverse() * Matrix;

        FbxAMatrix bindLocal = node->EvaluateLocalTransform();

        // FBX 행렬을 Unreal 형식으로 복사
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                joint.localBindPose.M[i][j] = static_cast<float>(bindLocal[i][j]);

        // FBX 행렬을 Unreal 형식으로 복사
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                joint.inverseBindPose.M[i][j] = static_cast<float>(InverseMatrix[i][j]);
    }
    // FbxPose를 통해 구하는 방법?
    else if (pose)
    {
        int index = pose->Find(node);
        if (index >= 0)
        {
            FbxMatrix bindLocal = pose->GetMatrix(index);
            FbxMatrix inverseBindLocal = bindLocal.Inverse();
            
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    joint.localBindPose.M[i][j] = static_cast<float>(bindLocal[i][j]);

            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    joint.inverseBindPose.M[i][j] = static_cast<float>(inverseBindLocal[i][j]);
        }
    }
    else
    {
        // 클러스터가 없는 경우에는 fallback으로 EvaluateLocalTransform 사용 (확인안됨)
        FbxAMatrix localMatrix = node->EvaluateLocalTransform();
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                joint.localBindPose.M[i][j] = static_cast<float>(localMatrix[i][j]);

        FbxAMatrix globalMatrix = node->EvaluateGlobalTransform();
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                joint.inverseBindPose.M[i][j] = static_cast<float>(globalMatrix[i][j]);
        joint.inverseBindPose = FMatrix::Inverse(joint.inverseBindPose);
    }

    FbxAMatrix LocalTransform = node->EvaluateLocalTransform();
    FMatrix Mat;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            Mat.M[i][j] = static_cast<float>(LocalTransform[i][j]);
    
    FTransform Transform;
    Transform.SetFromMatrix(Mat);

    joint.position = Transform.Translation;
    joint.rotation = Transform.Rotation;
    joint.scale = Transform.Scale3D;

    int thisIndex = fbxObject->skeleton.joints.Num();
    fbxObject->skeleton.joints.Add(joint);
    boneNameToIndex.Add(joint.name, thisIndex);

    // 재귀적으로 하위 노드 순회
    for (int i = 0; i < node->GetChildCount(); ++i)
    {
        LoadFbxSkeleton(fbxObject, node->GetChild(i), boneNameToIndex, thisIndex);
    }
}

void FFbxLoader::LoadSkinWeights(
    FbxNode* node,
    const TMap<FString, int>& boneNameToIndex,
    TMap<int, TArray<BoneWeights>>& OutBoneWeights
)
{
    FbxMesh* mesh = node->GetMesh();
    if (!mesh) return;
    
    int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int s = 0; s < skinCount; ++s)
    {
        FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(s, FbxDeformer::eSkin));
        int clustureCount = skin->GetClusterCount();
        for (int c = 0; c < clustureCount; ++c)
        {
            FbxCluster* cluster = skin->GetCluster(c);
            FbxNode* linkedBone = cluster->GetLink();
            if (!linkedBone)
                continue;

            FString boneName = linkedBone->GetName();
            int boneIndex = boneNameToIndex[boneName];

            int* indices = cluster->GetControlPointIndices();
            double* weights = cluster->GetControlPointWeights();
            int count = cluster->GetControlPointIndicesCount();
            for (int i = 0; i < count; ++i)
            {
                int ctrlIdx = indices[i];
                float weight = static_cast<float>(weights[i]);
                OutBoneWeights[ctrlIdx].Add({boneIndex, weight});
            }
            
        }
    } 
}

void FFbxLoader::LoadFBXMesh(
    FFbxSkeletalMesh* fbxObject,
    FbxNode* node,
    TMap<FString, int>& boneNameToIndex,
    TMap<int, TArray<BoneWeights>>& boneWeight
)
{
    FbxMesh* mesh = node->GetMesh();
    if (!mesh)
        return;

    FFbxMeshData meshData;
    meshData.name = node->GetName();
    
    int polygonCount = mesh->GetPolygonCount();
    FbxVector4* controlPoints = mesh->GetControlPoints();
    FbxLayerElementNormal* normalElement = mesh->GetElementNormal();
    FbxLayerElementTangent* tangentElement = mesh->GetElementTangent();
    FbxLayerElementUV* uvElement = mesh->GetElementUV();
    FbxLayerElementMaterial* materialElement = mesh->GetElementMaterial();

    FVector AABBmin(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector AABBmax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    TMap<FString, uint32> indexMap;
    TMap<int, TArray<uint32>> materialIndexToIndices;
    
    for (int polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex)
    {
        for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
        {
            FFbxVertex v;
            int controlPointIndex = mesh->GetPolygonVertex(polygonIndex, vertexIndex);
            int polygonVertexIndex = polygonIndex * 3 + vertexIndex;
            
            // vertex
            FbxVector4 pos = controlPoints[controlPointIndex];
            FVector convertPos(pos[0], pos[1], pos[2]);
            v.position = convertPos;
            
            AABBmin.X = std::min(AABBmin.X, v.position.X);
            AABBmin.Y = std::min(AABBmin.Y, v.position.Y);
            AABBmin.Z = std::min(AABBmin.Z, v.position.Z);
            AABBmax.X = std::max(AABBmax.X, v.position.X);
            AABBmax.Y = std::max(AABBmax.Y, v.position.Y);
            AABBmax.Z = std::max(AABBmax.Z, v.position.Z);
            
            // Normal
            FbxVector4 normal = {0, 0, 0};
            if (normalElement) {
                int index;
                switch (normalElement->GetMappingMode())
                {
                case FbxLayerElement::eByControlPoint:
                    index = controlPointIndex;
                    break;
                case FbxLayerElement::eByPolygonVertex:
                    index = polygonVertexIndex;
                    break;
                case FbxLayerElement::eByPolygon:
                    index = polygonIndex;
                    break;
                default:
                    index = 0;
                    break;
                }
                int normIdx = (normalElement->GetReferenceMode() == FbxLayerElement::eDirect)
                            ? index
                            : normalElement->GetIndexArray().GetAt(index);
                normal = normalElement->GetDirectArray().GetAt(normIdx);
            }
            FVector convertNormal(normal[0], normal[1], normal[2]);
            v.normal = convertNormal;

            // Tangent
            FbxVector4 tangent = {0, 0, 0};
            if (tangentElement)
            {
                int index;
                switch (tangentElement->GetMappingMode())
                {
                case FbxLayerElement::eByControlPoint:
                    index = controlPointIndex;
                    break;
                case FbxLayerElement::eByPolygonVertex:
                    index = polygonVertexIndex;
                    break;
                case FbxLayerElement::eByPolygon:
                    index = polygonIndex;
                    break;
                default:
                    index = 0;
                    break;
                }
                int tangentIdx = (tangentElement->GetReferenceMode() == FbxLayerElement::eDirect)
                                ? index
                                : tangentElement->GetIndexArray().GetAt(index);
                tangent = tangentElement->GetDirectArray().GetAt(tangentIdx);
            }
            FVector convertTangent = FVector(tangent[0], tangent[1], tangent[2]);
            v.tangent = convertTangent;

            // UV
            FbxVector2 uv = {0, 0};
            if (uvElement) {
                int uvIdx = mesh->GetTextureUVIndex(polygonIndex, vertexIndex);
                uv = uvElement->GetDirectArray().GetAt(uvIdx);
            }
            FVector2D convertUV(uv[0], 1.f - uv[1]);
            v.uv = convertUV;

            // Material & Subset
            if (materialElement)
            {
                const FbxLayerElementArrayTemplate<int>& indices = materialElement->GetIndexArray();
                v.materialIndex = indices.GetAt(polygonIndex);
                materialIndexToIndices[v.materialIndex].Add(static_cast<uint32>(controlPointIndex));
            }
            
            // Skin
            TArray<BoneWeights>* weights = boneWeight.Find(controlPointIndex);
            if (weights)
            {
                std::sort(weights->begin(), weights->end(), [](auto& a, auto& b)
                {
                    return a.weight > b.weight;
                });

                float total = 0.0f;
                for (int i = 0; i < 8 && i < weights->Num(); ++i)
                {
                    v.boneIndices[i] = (*weights)[i].jointIndex;
                    v.boneWeights[i] = (*weights)[i].weight;
                    total += (*weights)[i].weight;
                }


                // Normalize
                if (total > 0.f)
                {
                    for (int i = 0; i < 8; ++i)
                        v.boneWeights[i] /= total;
                }
            }

            // indices process
            std::stringstream ss;
            ss << GetData(convertPos.ToString()) << '|' << GetData(convertNormal.ToString()) << '|' << GetData(convertUV.ToString());
            FString key = ss.str();
            uint32 index;
            if (!indexMap.Contains(key))
            {
                index = meshData.vertices.Num();
                meshData.vertices.Add(v);
                indexMap[key] = index;
            } else
            {
                index = indexMap[key];
            }
            meshData.indices.Add(index);
        }
    }

    // subset 처리.
    uint32 accumIndex = 0;
    uint32 materialIndexOffset = fbxObject->material.Num();
    for (auto& [materialIndex, indices]: materialIndexToIndices)
    {
        FMaterialSubset subset;
        subset.IndexStart = static_cast<uint32>(accumIndex);
        subset.IndexCount = static_cast<uint32>(indices.Num());
        subset.MaterialIndex = materialIndexOffset + materialIndex;
        if (materialIndex < node->GetMaterialCount())
        {
            FbxSurfaceMaterial* mat = node->GetMaterial(materialIndex);
            if (mat)
                subset.MaterialName = mat->GetName();
        }
        accumIndex += indices.Num();
        meshData.subsetIndex.Add(fbxObject->materialSubsets.Num());
        fbxObject->materialSubsets.Add(subset);
    }

    // tangent 없을 경우 처리.
    if (tangentElement == nullptr)
    {
        for (int i = 0; i + 2 < meshData.indices.Num(); i += 3)
        {
            FFbxVertex& Vertex0 = meshData.vertices[meshData.indices[i]];
            FFbxVertex& Vertex1 = meshData.vertices[meshData.indices[i + 1]];
            FFbxVertex& Vertex2 = meshData.vertices[meshData.indices[i + 2]];

            CalculateTangent(Vertex0, Vertex1, Vertex2);
            CalculateTangent(Vertex1, Vertex2, Vertex0);
            CalculateTangent(Vertex2, Vertex0, Vertex1);
        }
        
    }

    // AABB 설정.
    fbxObject->AABBmin = AABBmin;
    fbxObject->AABBmax = AABBmax;

    fbxObject->mesh.Add(meshData);
}

void FFbxLoader::LoadFBXMaterials(
    FFbxSkeletalMesh* fbxObject,
    FbxNode* node
)
{
    if (!node)
        return;

    int materialCount = node->GetMaterialCount();
    
    for (int i = 0; i < materialCount; ++i)
    {
        FbxSurfaceMaterial* material = node->GetMaterial(i);

        UMaterial* materialInfo = FObjectFactory::ConstructObject<UMaterial>(nullptr);
        
        materialInfo->GetMaterialInfo().MaterialName = material->GetName();
        int reservedCount = static_cast<uint32>(EMaterialTextureSlots::MTS_MAX);
        for (int i = 0; i < reservedCount; ++i)
            materialInfo->GetMaterialInfo().TextureInfos.Add({});
        
        // normalMap
        FbxProperty normal = material->FindProperty(FbxSurfaceMaterial::sNormalMap);
        if (normal.IsValid())
        {
            FbxTexture* texture = normal.GetSrcObject<FbxTexture>();
            FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);
            if (fileTexture && CreateTextureFromFile(StringToWString(fileTexture->GetFileName()), false))
            {
                const uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Normal);
                FTextureInfo& slot = materialInfo->GetMaterialInfo().TextureInfos[SlotIdx];
                slot.TextureName = fileTexture->GetName();
                slot.TexturePath = StringToWString(fileTexture->GetFileName());
                slot.bIsSRGB = false;
                materialInfo->GetMaterialInfo().TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Normal);
            }
        }
        
        // diffuse
        FbxProperty diffuse = material->FindProperty(FbxSurfaceMaterial::sDiffuse);
        if (diffuse.IsValid())
        {
            FbxDouble3 color = diffuse.Get<FbxDouble3>();
            materialInfo->SetDiffuse(FVector(color[0], color[1], color[2]));
            
            FbxTexture* texture = diffuse.GetSrcObject<FbxTexture>();
            FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);
            if (fileTexture && CreateTextureFromFile(StringToWString(fileTexture->GetFileName()), true))
            {
                const uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Diffuse);
                FTextureInfo& slot = materialInfo->GetMaterialInfo().TextureInfos[SlotIdx];
                slot.TextureName = fileTexture->GetName();
                slot.TexturePath = StringToWString(fileTexture->GetFileName());
                slot.bIsSRGB = true;
                materialInfo->GetMaterialInfo().TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Diffuse);
            }
        }
        
        // ambient
        FbxProperty ambient = material->FindProperty(FbxSurfaceMaterial::sAmbient);
        if (ambient.IsValid())
        {
            FbxDouble3 color = ambient.Get<FbxDouble3>();
            materialInfo->SetAmbient(FVector(color[0], color[1], color[2]));
            
            FbxTexture* texture = ambient.GetSrcObject<FbxTexture>();
            FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);
            if (fileTexture && CreateTextureFromFile(StringToWString(fileTexture->GetFileName()), true))
            {
                const uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Ambient);
                FTextureInfo& slot = materialInfo->GetMaterialInfo().TextureInfos[SlotIdx];
                slot.TextureName = fileTexture->GetName();
                slot.TexturePath = StringToWString(fileTexture->GetFileName());
                slot.bIsSRGB = true;
                materialInfo->GetMaterialInfo().TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Ambient);
            }

        }

        // specular
        FbxProperty specular = material->FindProperty(FbxSurfaceMaterial::sSpecular);
        if (ambient.IsValid())
        {
            FbxDouble3 color = specular.Get<FbxDouble3>();
            materialInfo->SetSpecular(FVector(color[0], color[1], color[2]));
            
            FbxTexture* texture = specular.GetSrcObject<FbxTexture>();
            FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);
            if (fileTexture && CreateTextureFromFile(StringToWString(fileTexture->GetFileName()), true))
            {
                const uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Specular);
                FTextureInfo& slot = materialInfo->GetMaterialInfo().TextureInfos[SlotIdx];
                slot.TextureName = fileTexture->GetName();
                slot.TexturePath = StringToWString(fileTexture->GetFileName());
                slot.bIsSRGB = true;
                materialInfo->GetMaterialInfo().TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Specular);
            }
        }

        // emissive
        FbxProperty emissive = material->FindProperty(FbxSurfaceMaterial::sEmissive);
        if (ambient.IsValid())
        {
            FbxDouble3 color = emissive.Get<FbxDouble3>();
            materialInfo->SetEmissive(FVector(color[0], color[1], color[2]));
            
            FbxTexture* texture = emissive.GetSrcObject<FbxTexture>();
            FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);
            if (fileTexture && CreateTextureFromFile(StringToWString(fileTexture->GetFileName()), true))
            {
                const uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Emissive);
                FTextureInfo& slot = materialInfo->GetMaterialInfo().TextureInfos[SlotIdx];
                slot.TextureName = fileTexture->GetName();
                slot.TexturePath = StringToWString(fileTexture->GetFileName());
                slot.bIsSRGB = true;
                
                materialInfo->GetMaterialInfo().TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Emissive);
            }
        }
        
        fbxObject->material.Add(materialInfo);
    }
}

bool FFbxLoader::CreateTextureFromFile(const FWString& Filename, bool bIsSRGB)
{
    if (FEngineLoop::ResourceManager.GetTexture(Filename))
    {
        return true;
    }

    HRESULT hr = FEngineLoop::ResourceManager.LoadTextureFromFile(FEngineLoop::GraphicDevice.Device, Filename.c_str(), bIsSRGB);

    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

void FFbxLoader::CalculateTangent(FFbxVertex& PivotVertex, const FFbxVertex& Vertex1, const FFbxVertex& Vertex2)
{
    const float s1 = Vertex1.uv.X - PivotVertex.uv.X;
    const float t1 = Vertex1.uv.Y - PivotVertex.uv.Y;
    const float s2 = Vertex2.uv.X - PivotVertex.uv.X;
    const float t2 = Vertex2.uv.Y - PivotVertex.uv.Y;
    const float E1x = Vertex1.position.X - PivotVertex.position.X;
    const float E1y = Vertex1.position.Y - PivotVertex.position.Y;
    const float E1z = Vertex1.position.Z - PivotVertex.position.Z;
    const float E2x = Vertex2.position.X - PivotVertex.position.X;
    const float E2y = Vertex2.position.Y - PivotVertex.position.Y;
    const float E2z = Vertex2.position.Z - PivotVertex.position.Z;

    const float Denominator = s1 * t2 - s2 * t1;
    FVector Tangent(1, 0, 0);
    FVector BiTangent(0, 1, 0);
    FVector Normal(PivotVertex.normal.X, PivotVertex.normal.Y, PivotVertex.normal.Z);
    
    if (FMath::Abs(Denominator) > SMALL_NUMBER)
    {
        // 정상적인 계산 진행
        const float f = 1.f / Denominator;
        
        const float Tx = f * (t2 * E1x - t1 * E2x);
        const float Ty = f * (t2 * E1y - t1 * E2y);
        const float Tz = f * (t2 * E1z - t1 * E2z);
        Tangent = FVector(Tx, Ty, Tz).GetSafeNormal();

        const float Bx = f * (-s2 * E1x + s1 * E2x);
        const float By = f * (-s2 * E1y + s1 * E2y);
        const float Bz = f * (-s2 * E1z + s1 * E2z);
        BiTangent = FVector(Bx, By, Bz).GetSafeNormal();
    }
    else
    {
        // 대체 탄젠트 계산 방법
        // 방법 1: 다른 방향에서 탄젠트 계산 시도
        FVector Edge1(E1x, E1y, E1z);
        FVector Edge2(E2x, E2y, E2z);
    
        // 기하학적 접근: 두 에지 사이의 각도 이등분선 사용
        Tangent = (Edge1.GetSafeNormal() + Edge2.GetSafeNormal()).GetSafeNormal();
    
        // 만약 두 에지가 평행하거나 반대 방향이면 다른 방법 사용
        if (Tangent.IsNearlyZero())
        {
            // TODO: 기본 축 방향 중 하나 선택 (메시의 주 방향에 따라 선택)
            Tangent = FVector(1.0f, 0.0f, 0.0f);
        }
    }

    Tangent = (Tangent - Normal * FVector::DotProduct(Normal, Tangent)).GetSafeNormal();
    
    const float Sign = (FVector::DotProduct(FVector::CrossProduct(Normal, Tangent), BiTangent) < 0.f) ? -1.f : 1.f;

    PivotVertex.tangent.X = Tangent.X;
    PivotVertex.tangent.Y = Tangent.Y;
    PivotVertex.tangent.Z = Tangent.Z;
    PivotVertex.tangent.W = Sign;
}

bool FFbxManager::SaveFBXToBinary(const FWString& FilePath, int64_t LastModifiedTime, const FFbxSkeletalMesh& FBXObject)
{
    /** File Open */
    std::ofstream File(FilePath, std::ios::binary);

    if (!File.is_open())
    {
        assert("CAN'T SAVE FBX FILE TO BINARY");
        return false;
    }

    /** Modified */
    File.write(reinterpret_cast<const char*>(&LastModifiedTime), sizeof(&LastModifiedTime));

    /** FBX Name */
    Serializer::WriteFString(File, FBXObject.name);

    /** FBX Mesh */
    uint32 MeshCount = FBXObject.mesh.Num();
    File.write(reinterpret_cast<const char*>(&MeshCount), sizeof(MeshCount));
    for (const FFbxMeshData& MeshData : FBXObject.mesh)
    {
        // Mesh Vertices
        uint32 VertexCount = MeshData.vertices.Num();
        File.write(reinterpret_cast<const char*>(&VertexCount), sizeof(VertexCount));
        if (VertexCount > 0)
        {
            File.write(reinterpret_cast<const char*>(MeshData.vertices.GetData()), sizeof(FFbxVertex) * VertexCount);
        }

        // Mesh Indices
        uint32 IndexCount = MeshData.indices.Num();
        File.write(reinterpret_cast<const char*>(&IndexCount), sizeof(IndexCount));
        if (IndexCount > 0)
        {
            File.write(reinterpret_cast<const char*>(MeshData.indices.GetData()), sizeof(uint32) * IndexCount);
        }

        // Subset
        uint32 SubIndexCount = MeshData.subsetIndex.Num();
        File.write(reinterpret_cast<const char*>(&SubIndexCount), sizeof(SubIndexCount));
        if (SubIndexCount > 0)
        {
            File.write(reinterpret_cast<const char*>(MeshData.subsetIndex.GetData()), sizeof(uint32) * SubIndexCount);
        }

        // Name
        Serializer::WriteFString(File, MeshData.name);
    }
    
    /** FBX Skeleton */
    uint32 JointCount = FBXObject.skeleton.joints.Num();
    File.write(reinterpret_cast<const char*>(&JointCount), sizeof(JointCount));
    for (const FFbxJoint& Joint : FBXObject.skeleton.joints)
    {
        // Joint Name
        Serializer::WriteFString(File, Joint.name);

        // Parent index
        File.write(reinterpret_cast<const char*>(&Joint.parentIndex), sizeof(Joint.parentIndex));

        // Local bind pose
        File.write(reinterpret_cast<const char*>(&Joint.localBindPose), sizeof(Joint.localBindPose));

        // Inverse bind pose
        File.write(reinterpret_cast<const char*>(&Joint.inverseBindPose), sizeof(Joint.inverseBindPose));

        // Position
        File.write(reinterpret_cast<const char*>(&Joint.position), sizeof(Joint.position));

        // Rotation
        File.write(reinterpret_cast<const char*>(&Joint.rotation), sizeof(Joint.rotation));

        // Scale
        File.write(reinterpret_cast<const char*>(&Joint.scale), sizeof(Joint.scale));
    }

    /** FBX UMaterial */
    uint32 MaterialCount = FBXObject.material.Num();
    File.write(reinterpret_cast<const char*>(&MaterialCount), sizeof(MaterialCount));
    for (UMaterial* const Material : FBXObject.material)
    {
        bool bIsValidMaterial = (Material != nullptr);
        File.write(reinterpret_cast<const char*>(&bIsValidMaterial), sizeof(bIsValidMaterial));
        
        if (bIsValidMaterial)
        {
            const FMaterialInfo& MaterialInfo = Material->GetMaterialInfo();

            // MaterialInfo.MaterialName (FString)
            Serializer::WriteFString(File, MaterialInfo.MaterialName);
            
            // MaterialInfo.TextureFlag (uint32)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.TextureFlag), sizeof(MaterialInfo.TextureFlag));
            
            // MaterialInfo.bTransparent (bool)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.bTransparent), sizeof(MaterialInfo.bTransparent));
            
            // MaterialInfo.DiffuseColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.DiffuseColor), sizeof(MaterialInfo.DiffuseColor));
            
            // MaterialInfo.SpecularColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.SpecularColor), sizeof(MaterialInfo.SpecularColor));
            
            // MaterialInfo.AmbientColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.AmbientColor), sizeof(MaterialInfo.AmbientColor));
            
            // MaterialInfo.EmissiveColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.EmissiveColor), sizeof(MaterialInfo.EmissiveColor));
            
            // MaterialInfo.SpecularExponent (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.SpecularExponent), sizeof(MaterialInfo.SpecularExponent));
            
            // MaterialInfo.IOR (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.IOR), sizeof(MaterialInfo.IOR));
            
            // MaterialInfo.Transparency (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Transparency), sizeof(MaterialInfo.Transparency));
            
            // MaterialInfo.BumpMultiplier (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.BumpMultiplier), sizeof(MaterialInfo.BumpMultiplier));
            
            // MaterialInfo.IlluminanceModel (uint32)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.IlluminanceModel), sizeof(MaterialInfo.IlluminanceModel));
            
            // MaterialInfo.Metallic (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Metallic), sizeof(MaterialInfo.Metallic));
            
            // MaterialInfo.Roughness (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Roughness), sizeof(MaterialInfo.Roughness));
            
            // MaterialInfo.AmbientOcclusion (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.AmbientOcclusion), sizeof(MaterialInfo.AmbientOcclusion));
            
            // MaterialInfo.ClearCoat (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.ClearCoat), sizeof(MaterialInfo.ClearCoat));
            
            // MaterialInfo.Sheen (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Sheen), sizeof(MaterialInfo.Sheen));

            // MaterialInfo.TextureInfos (TArray<FTextureInfo>)
            uint32 TextureInfoCount = MaterialInfo.TextureInfos.Num();
            File.write(reinterpret_cast<const char*>(&TextureInfoCount), sizeof(TextureInfoCount));
            for (const FTextureInfo& texInfo : MaterialInfo.TextureInfos)
            {
                Serializer::WriteFString(File, texInfo.TextureName);
                 Serializer::WriteFWString(File, texInfo.TexturePath);
                File.write(reinterpret_cast<const char*>(&texInfo.bIsSRGB), sizeof(texInfo.bIsSRGB));
            }
        }
    }

    /** FBX Material Subset */
    uint32 SubsetCount = FBXObject.materialSubsets.Num();
    File.write(reinterpret_cast<const char*>(&SubsetCount), sizeof(SubsetCount));
    for (const FMaterialSubset& Subset : FBXObject.materialSubsets)
    {
        File.write(reinterpret_cast<const char*>(&Subset.IndexStart), sizeof(Subset.IndexStart));
        File.write(reinterpret_cast<const char*>(&Subset.IndexCount), sizeof(Subset.IndexCount));
        File.write(reinterpret_cast<const char*>(&Subset.MaterialIndex), sizeof(Subset.MaterialIndex));
        Serializer::WriteFString(File, Subset.MaterialName);
    }
    
    /** FBX AABB */
    File.write(reinterpret_cast<const char*>(&FBXObject.AABBmin), sizeof(FVector));
    File.write(reinterpret_cast<const char*>(&FBXObject.AABBmax), sizeof(FVector));
    
    File.close();
    return true;
}

bool FFbxManager::LoadFBXFromBinary(const FWString& FilePath, int64_t LastModifiedTime, FFbxSkeletalMesh& OutFBXObject)
{
    std::ifstream File(FilePath, std::ios::binary);
    if (!File.is_open())
    {
        assert("CAN'T OPEN FBX BINARY FILE");
        return false;
    }

    /** Modified Check */
    int64_t FileLastModifiedTime;
    File.read(reinterpret_cast<char*>(&FileLastModifiedTime), sizeof(FileLastModifiedTime));

    // File is changed.
    if (LastModifiedTime != FileLastModifiedTime)
    {
        return false;
    }

    /** FBX Name */
    Serializer::ReadFString(File, OutFBXObject.name);
    
    /** FBX Mesh */
    uint32 MeshCount;
    File.read(reinterpret_cast<char*>(&MeshCount), sizeof(MeshCount));
    OutFBXObject.mesh.Reserve(MeshCount); // 미리 메모리 할당
    for (uint32 i = 0; i < MeshCount; ++i)
    {
        FFbxMeshData MeshData;

        // Mesh Vertices
        uint32 VertexCount;
        File.read(reinterpret_cast<char*>(&VertexCount), sizeof(VertexCount));
        if (VertexCount > 0)
        {
            MeshData.vertices.SetNum(VertexCount); // 크기 설정
            File.read(reinterpret_cast<char*>(MeshData.vertices.GetData()), sizeof(FFbxVertex) * VertexCount);
        }

        // Mesh Indices
        uint32 IndexCount;
        File.read(reinterpret_cast<char*>(&IndexCount), sizeof(IndexCount));
        if (IndexCount > 0)
        {
            MeshData.indices.SetNum(IndexCount); // 크기 설정
            File.read(reinterpret_cast<char*>(MeshData.indices.GetData()), sizeof(uint32) * IndexCount);
        }
        
        // Subset
        uint32 SubIndexCount;
        File.read(reinterpret_cast<char*>(&SubIndexCount), sizeof(SubIndexCount));
        if (SubIndexCount > 0)
        {
            MeshData.subsetIndex.SetNum(SubIndexCount); // 크기 설정
            File.read(reinterpret_cast<char*>(MeshData.subsetIndex.GetData()), sizeof(uint32) * SubIndexCount);
        }

        // Name
        Serializer::ReadFString(File, MeshData.name);
        OutFBXObject.mesh.Add(std::move(MeshData));
    }

    /** FBX Skeleton */
    uint32 JointCount;
    File.read(reinterpret_cast<char*>(&JointCount), sizeof(JointCount));
    OutFBXObject.skeleton.joints.Reserve(JointCount); // 미리 메모리 할당
    for (uint32 i = 0; i < JointCount; ++i)
    {
        FFbxJoint Joint;

        // Joint Name
        Serializer::ReadFString(File, Joint.name);

        // Parent index
        File.read(reinterpret_cast<char*>(&Joint.parentIndex), sizeof(Joint.parentIndex));

        // Local bind pose
        File.read(reinterpret_cast<char*>(&Joint.localBindPose), sizeof(Joint.localBindPose));

        // Inverse bind pose
        File.read(reinterpret_cast<char*>(&Joint.inverseBindPose), sizeof(Joint.inverseBindPose));

        // Position
        File.read(reinterpret_cast<char*>(&Joint.position), sizeof(Joint.position));

        // Rotation
        File.read(reinterpret_cast<char*>(&Joint.rotation), sizeof(Joint.rotation));

        // Scale
        File.read(reinterpret_cast<char*>(&Joint.scale), sizeof(Joint.scale));
        
        OutFBXObject.skeleton.joints.Add(std::move(Joint));
    }
    
    /** FBX UMaterial */
    uint32 MaterialCount;
    File.read(reinterpret_cast<char*>(&MaterialCount), sizeof(MaterialCount));
    OutFBXObject.material.Reserve(MaterialCount); // 미리 메모리 할당
    for (uint32 i = 0; i < MaterialCount; ++i)
    {
        bool bIsValidMaterial;
        File.read(reinterpret_cast<char*>(&bIsValidMaterial), sizeof(bIsValidMaterial));
        
        if (bIsValidMaterial)
        {
            UMaterial* NewMaterial = new UMaterial(); // UMaterial 객체 생성
            FMaterialInfo MaterialInfo;

            // MaterialInfo.MaterialName (FString)
            Serializer::ReadFString(File, MaterialInfo.MaterialName);
            
            // MaterialInfo.TextureFlag (uint32)
            File.read(reinterpret_cast<char*>(&MaterialInfo.TextureFlag), sizeof(MaterialInfo.TextureFlag));
            
            // MaterialInfo.bTransparent (bool)
            File.read(reinterpret_cast<char*>(&MaterialInfo.bTransparent), sizeof(MaterialInfo.bTransparent));
            
            // MaterialInfo.DiffuseColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.DiffuseColor), sizeof(MaterialInfo.DiffuseColor));
            
            // MaterialInfo.SpecularColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.SpecularColor), sizeof(MaterialInfo.SpecularColor));
            
            // MaterialInfo.AmbientColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.AmbientColor), sizeof(MaterialInfo.AmbientColor));
            
            // MaterialInfo.EmissiveColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.EmissiveColor), sizeof(MaterialInfo.EmissiveColor));
            
            // MaterialInfo.SpecularExponent (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.SpecularExponent), sizeof(MaterialInfo.SpecularExponent));
            
            // MaterialInfo.IOR (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.IOR), sizeof(MaterialInfo.IOR));
            
            // MaterialInfo.Transparency (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Transparency), sizeof(MaterialInfo.Transparency));
            
            // MaterialInfo.BumpMultiplier (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.BumpMultiplier), sizeof(MaterialInfo.BumpMultiplier));
            
            // MaterialInfo.IlluminanceModel (uint32)
            File.read(reinterpret_cast<char*>(&MaterialInfo.IlluminanceModel), sizeof(MaterialInfo.IlluminanceModel));
            
            // MaterialInfo.Metallic (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Metallic), sizeof(MaterialInfo.Metallic));
            
            // MaterialInfo.Roughness (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Roughness), sizeof(MaterialInfo.Roughness));
            
            // MaterialInfo.AmbientOcclusion (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.AmbientOcclusion), sizeof(MaterialInfo.AmbientOcclusion));
            
            // MaterialInfo.ClearCoat (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.ClearCoat), sizeof(MaterialInfo.ClearCoat));
            
            // MaterialInfo.Sheen (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Sheen), sizeof(MaterialInfo.Sheen));

            // MaterialInfo.TextureInfos (TArray<FTextureInfo>)
            uint32 TextureInfoCount;
            File.read(reinterpret_cast<char*>(&TextureInfoCount), sizeof(TextureInfoCount));
            MaterialInfo.TextureInfos.Reserve(TextureInfoCount); // 미리 메모리 할당
            for (uint32 j = 0; j < TextureInfoCount; ++j)
            {
                FTextureInfo TexInfo;
                Serializer::ReadFString(File, TexInfo.TextureName);
                Serializer::ReadFWString(File, TexInfo.TexturePath);
                File.read(reinterpret_cast<char*>(&TexInfo.bIsSRGB), sizeof(TexInfo.bIsSRGB));
                MaterialInfo.TextureInfos.Add(std::move(TexInfo));
            }
            NewMaterial->SetMaterialInfo(MaterialInfo);
            OutFBXObject.material.Add(NewMaterial);
        }
        else
        {
            OutFBXObject.material.Add(nullptr);
        }
    }

    /** FBX Material Subset */
    uint32 SubsetCount;
    File.read(reinterpret_cast<char*>(&SubsetCount), sizeof(SubsetCount));
    OutFBXObject.materialSubsets.Reserve(SubsetCount); // 미리 메모리 할당
    for (uint32 i = 0; i < SubsetCount; ++i)
    {
        FMaterialSubset Subset;
        File.read(reinterpret_cast<char*>(&Subset.IndexStart), sizeof(Subset.IndexStart));
        File.read(reinterpret_cast<char*>(&Subset.IndexCount), sizeof(Subset.IndexCount));
        File.read(reinterpret_cast<char*>(&Subset.MaterialIndex), sizeof(Subset.MaterialIndex));
        Serializer::ReadFString(File, Subset.MaterialName);
        OutFBXObject.materialSubsets.Add(std::move(Subset));
    }
    
    /** FBX AABB */
    File.read(reinterpret_cast<char*>(&OutFBXObject.AABBmin), sizeof(FVector));
    File.read(reinterpret_cast<char*>(&OutFBXObject.AABBmax), sizeof(FVector));
    
    File.close();
    return true;
}
