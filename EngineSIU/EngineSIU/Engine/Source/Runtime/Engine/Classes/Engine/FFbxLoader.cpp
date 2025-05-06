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

FSkinnedMesh* FFbxLoader::ParseFBX(const FString& FBXFilePath)
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

    FbxAxisSystem targetAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);
    targetAxisSystem.ConvertScene(scene);
    bool bIsImported = importer->Import(scene);
    importer->Destroy();
    if (!bIsImported)
    {
        return nullptr;   
    }
    
    FbxGeometryConverter converter(GetFbxManager());
    converter.Triangulate(scene, true);

    FSkinnedMesh* result = LoadFBXObject(scene);
    scene->Destroy();
    result->name = FBXFilePath;
    fbxMap[FBXFilePath] = result;
    return result;
}

FSkinnedMesh* FFbxLoader::GetFbxObject(const FString& filename)
{
    if (!fbxMap.Contains(filename))
        ParseFBX(filename);
    return fbxMap[filename];
}

FSkinnedMesh* FFbxLoader::LoadFBXObject(FbxScene* InFbxInfo)
{
    FSkinnedMesh* result = new FSkinnedMesh();

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

    // // parse materials
    // for (auto& node: meshes)
    // {
    // }
    
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
    FSkinnedMesh* fbxObject,
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
    FSkinnedMesh* fbxObject,
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
            
            // vertex
            int controlPointIndex = mesh->GetPolygonVertex(polygonIndex, vertexIndex);
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
                int normIdx = (normalElement->GetReferenceMode() == FbxLayerElement::eDirect)
                            ? polygonIndex * 3 + vertexIndex
                            : normalElement->GetIndexArray().GetAt(polygonIndex * 3 + vertexIndex);
                normal = normalElement->GetDirectArray().GetAt(normIdx);
            }
            FVector convertNormal(normal[0], normal[1], normal[2]);
            v.normal = convertNormal;

            // Tangent
            FbxVector4 tangent = {0, 0, 0};
            if (tangentElement)
            {
                int tangentIdx = (tangentElement->GetReferenceMode() == FbxLayerElement::eDirect)
                                ? polygonIndex * 3 + vertexIndex
                                : tangentElement->GetIndexArray().GetAt(polygonIndex * 3 + vertexIndex);
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
            uint32 i0 = meshData.indices[i];
            uint32 i1 = meshData.indices[i + 1];
            uint32 i2 = meshData.indices[i + 2];

            FFbxVertex& v0 = meshData.vertices[i0];
            FFbxVertex& v1 = meshData.vertices[i1];
            FFbxVertex& v2 = meshData.vertices[i2];
            
            FVector dp1 = v1.position - v0.position;
            FVector dp2 = v2.position - v0.position;
            FVector2D duv1 = v1.uv - v0.uv;
            FVector2D duv2 = v2.uv - v0.uv;

            float r = 1.0f / (duv1.X * duv2.Y - duv1.Y * duv2.X);
            FVector tangent = (dp1 * duv2.Y - dp2 * duv1.Y) * r;

            v0.tangent += tangent;
            v1.tangent += tangent;
            v2.tangent += tangent;
        }
        for (int i = 0; i < meshData.vertices.Num(); ++i)
            meshData.vertices[i].tangent = meshData.vertices[i].tangent * 1 / meshData.vertices[i].tangent.xyz().Length();
    }

    // AABB 설정.
    fbxObject->AABBmin = AABBmin;
    fbxObject->AABBmax = AABBmax;

    fbxObject->mesh.Add(meshData);
}

void FFbxLoader::LoadFBXMaterials(
    FSkinnedMesh* fbxObject,
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
