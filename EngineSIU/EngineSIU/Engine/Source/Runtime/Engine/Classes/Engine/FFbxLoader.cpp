#include "FFbxLoader.h"

#include <sstream>

#include "FbxObject.h"
#include "UObject/ObjectFactory.h"
#include "Components/Material/Material.h"

struct BoneWeights
{
    int jointIndex;
    float weight;
};

FSkeletalMesh* FFbxLoader::ParseFBX(const FString& FBXFilePath)
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

    FSkeletalMesh* result = LoadFBXObject(scene);
    scene->Destroy();
    result->name = FBXFilePath;
    fbxMap[FBXFilePath] = result;
    return result;
}

FSkeletalMesh* FFbxLoader::GetFbxObject(const FString& filename)
{
    if (!fbxMap.Contains(filename))
        ParseFBX(filename);
    return fbxMap[filename];
}

FSkeletalMesh* FFbxLoader::LoadFBXObject(FbxScene* InFbxInfo)
{
    FSkeletalMesh* result = new FSkeletalMesh();

    TMap<int, TArray<BoneWeights>> weightMap;
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
    for (auto& node: meshes)
    {
        LoadSkinWeights(node, boneNameToIndex, weightMap);
    }

    // parse meshes & material
    for (auto& node: meshes)
    {
        LoadFBXMesh(result, node, boneNameToIndex, weightMap);
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


void FFbxLoader::LoadFbxSkeleton(
    FSkeletalMesh* fbxObject,
    FbxNode* node,
    TMap<FString, int>& boneNameToIndex,
    int parentIndex = -1
)
{
    if (!node)
        return;

    if (boneNameToIndex.Contains(node->GetName()))
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

    FbxAMatrix m = node->EvaluateGlobalTransform();
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            joint.localBindPose.M[i][j] = m[i][j];
        }
    }
    joint.inverseBindPose = FMatrix::Inverse(joint.localBindPose);
    auto t = node->LclTranslation.Get();
    joint.position = FVector(t[0], t[1], t[2]);
    auto r = node->LclRotation.Get();
    joint.rotation = FQuat::CreateRotation(
        FMath::RadiansToDegrees(r[0]),
        FMath::RadiansToDegrees(r[1]),
        FMath::RadiansToDegrees(r[2])
    );
    auto s = node->LclScaling.Get();
    joint.scale = FVector(s[0], s[1], s[2]);

    int thisIndex = fbxObject->skeleton.joints.Num();
    
    fbxObject->skeleton.joints.Add(joint);
    boneNameToIndex.Add(joint.name, thisIndex);
    
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
    FSkeletalMesh* fbxObject,
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
                for (int i = 0; i < 4 && i < weights->Num(); ++i)
                {
                    v.boneIndices[i] = (*weights)[i].jointIndex;
                    v.boneWeights[i] = (*weights)[i].weight;
                    total += (*weights)[i].weight;
                }


                // Normalize
                if (total > 0.f)
                {
                    for (int i = 0; i < 4; ++i)
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
    FSkeletalMesh* fbxObject,
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
