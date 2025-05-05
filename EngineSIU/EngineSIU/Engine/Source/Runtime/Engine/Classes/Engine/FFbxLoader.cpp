#include "FFbxLoader.h"

#include <sstream>

#include "FbxObject.h"
#include "UObject/ObjectFactory.h"

struct BoneWeights
{
    int jointIndex;
    float weight;
};

FFbxObject* FFbxLoader::ParseFBX(const FString& FBXFilePath)
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
    
    FFbxObject* result = LoadFBXObject(scene);
    scene->Destroy();
    fbxMap[FBXFilePath] = result;
    return result;
}

FFbxObject* FFbxLoader::LoadFBXObject(FbxScene* InFbxInfo)
{
    FFbxObject* result = new FFbxObject();

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

    // parse meshes
    for (auto& node: meshes)
    {
        LoadFBXMesh(result, node, boneNameToIndex, weightMap);
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
    FFbxObject* fbxObject,
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
    FFbxObject* fbxObject,
    FbxNode* node,
    TMap<FString, int>& boneNameToIndex,
    TMap<int, TArray<BoneWeights>>& boneWeight
)
{
    FbxMesh* mesh = node->GetMesh();
    if (!mesh) return;

    int polygonCount = mesh->GetPolygonCount();
    FbxVector4* controlPoints = mesh->GetControlPoints();
    FbxLayerElementNormal* normalElement = mesh->GetElementNormal();
    FbxLayerElementUV* uvElement = mesh->GetElementUV();

    std::unordered_map<std::string, uint32> indexMap;

    for (int polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex)
    {
        for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
        {
            FFbxVertex v;
            
            // vertex
            int controlPointIndex = mesh->GetPolygonVertex(polygonIndex, vertexIndex);
            FbxVector4 pos = controlPoints[controlPointIndex];
            FVector convertPos(pos[0], pos[1], pos[2]);
            v.vertex = convertPos;
            
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

            // UV
            FbxVector2 uv = {0, 0};
            if (uvElement) {
                int uvIdx = mesh->GetTextureUVIndex(polygonIndex, vertexIndex);
                uv = uvElement->GetDirectArray().GetAt(uvIdx);
            }
            FVector2D convertUV(uv[0], uv[1]);
            v.uv = convertUV;

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
            std::string key = ss.str();
            uint32 index;
            if (!indexMap.contains(key))
            {
                index = fbxObject->mesh.vertices.Num();
                fbxObject->mesh.vertices.Add(v);
                indexMap[key] = index;
            } else
            {
                index = indexMap[key];
            }
            fbxObject->mesh.indices.Add(index);
        }
    }
}
