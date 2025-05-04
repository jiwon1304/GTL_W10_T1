#pragma once
#include <fbxsdk.h>

#include "Container/Map.h"
#include "Container/String.h"

struct FFbxObject;

struct FFbxLoader
{
    static FFbxObject* ParseFBX(const FString& FBXFilePath);
    static FbxManager* GetFbxManager();
private:
    static FbxIOSettings* GetFbxIOSettings();
    static void LoadFBXMesh(FFbxObject* fbxObject, FbxNode* node);
    static FFbxObject* LoadFBXObject(FbxScene* InFbxInfo);
    
    inline static TMap<FString, FFbxObject*> fbxMap;
};

