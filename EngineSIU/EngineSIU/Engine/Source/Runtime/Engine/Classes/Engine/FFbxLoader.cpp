#include "FFbxLoader.h"

#include <sstream>

#include "FbxObject.h"
#include "UObject/ObjectFactory.h"

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
    std::function<void(FbxNode*)> Traverse = [&result, &Traverse](FbxNode* Node)
    {   if (!Node) return;
        FbxNodeAttribute* attr = Node->GetNodeAttribute();
        if (attr)
        {
            switch (attr->GetAttributeType())
            {
            case FbxNodeAttribute::eUnknown:
                break;
            case FbxNodeAttribute::eNull:
                break;
            case FbxNodeAttribute::eMarker:
                break;
            case FbxNodeAttribute::eSkeleton:
                break;
            case FbxNodeAttribute::eMesh:
                LoadFBXMesh(result, Node);
                break;
            case FbxNodeAttribute::eNurbs:
                break;
            case FbxNodeAttribute::ePatch:
                break;
            case FbxNodeAttribute::eCamera:
                break;
            case FbxNodeAttribute::eCameraStereo:
                break;
            case FbxNodeAttribute::eCameraSwitcher:
                break;
            case FbxNodeAttribute::eLight:
                break;
            case FbxNodeAttribute::eOpticalReference:
                break;
            case FbxNodeAttribute::eOpticalMarker:
                break;
            case FbxNodeAttribute::eNurbsCurve:
                break;
            case FbxNodeAttribute::eTrimNurbsSurface:
                break;
            case FbxNodeAttribute::eBoundary:
                break;
            case FbxNodeAttribute::eNurbsSurface:
                break;
            case FbxNodeAttribute::eShape:
                break;
            case FbxNodeAttribute::eLODGroup:
                break;
            case FbxNodeAttribute::eSubDiv:
                break;
            case FbxNodeAttribute::eCachedEffect:
                break;
            case FbxNodeAttribute::eLine:
                break;
            }
        }

        for (int i = 0; i < Node->GetChildCount(); ++i)
        {
            Traverse(Node->GetChild(i));
        }
    };
    Traverse(InFbxInfo->GetRootNode());
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

void FFbxLoader::LoadFBXMesh(FFbxObject* fbxObject, FbxNode* node)
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
            // vertex
            int controlPointIndex = mesh->GetPolygonVertex(polygonIndex, vertexIndex);
            FbxVector4 pos = controlPoints[controlPointIndex];
            FVector convertPos(pos[0], pos[1], pos[2]);
            
            // Normal
            FbxVector4 normal = {0, 0, 0};
            if (normalElement) {
                int normIdx = (normalElement->GetReferenceMode() == FbxLayerElement::eDirect)
                            ? polygonIndex * 3 + vertexIndex
                            : normalElement->GetIndexArray().GetAt(polygonIndex * 3 + vertexIndex);
                normal = normalElement->GetDirectArray().GetAt(normIdx);
            }
            FVector convertNormal(normal[0], normal[1], normal[2]);

            // UV
            FbxVector2 uv = {0, 0};
            if (uvElement) {
                int uvIdx = mesh->GetTextureUVIndex(polygonIndex, vertexIndex);
                uv = uvElement->GetDirectArray().GetAt(uvIdx);
            }
            FVector2D convertUV(uv[0], uv[1]);

            // indices process
            std::stringstream ss;
            ss << GetData(convertPos.ToString()) << '|' << GetData(convertNormal.ToString()) << '|' << GetData(convertUV.ToString());
            std::string key = ss.str();
            uint32 index;
            if (!indexMap.contains(key))
            {
                index = fbxObject->mesh.vertices.Num();
                fbxObject->mesh.vertices.Add(convertPos);
                fbxObject->mesh.normals.Add(convertNormal);
                fbxObject->mesh.uvs.Add(convertUV);
                indexMap[key] = index;
            } else
            {
                index = indexMap[key];
            }
            fbxObject->mesh.indices.Add(index);
        }
    }
}
