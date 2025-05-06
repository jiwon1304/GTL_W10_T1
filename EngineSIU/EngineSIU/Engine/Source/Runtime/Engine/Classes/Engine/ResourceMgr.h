#pragma once
#include <memory>
#include "Texture.h"
#include "Container/Map.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"

class FRenderer;
class FGraphicsDevice;
class UStaticMeshTest;
class USkeletalMesh;

class FResourceMgr
{

public:
    void Initialize(FRenderer* renderer, FGraphicsDevice* device);
    void Release(FRenderer* renderer);
    HRESULT LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename, bool bIsSRGB = true);
    HRESULT LoadTextureFromDDS(ID3D11Device* device, ID3D11DeviceContext* context, const wchar_t* filename);

    std::shared_ptr<FTexture> GetTexture(const FWString& name) const;
    UStaticMeshTest* GetStaticMesh(FName InName) const;
    USkeletalMesh* GetSkeletalMesh(FName InName) const;

    void AddAssignStaticMeshMap(FName InName, UStaticMeshTest* InStaticMesh);
    void AddAssignSkeletalMeshMap(FName InName, USkeletalMesh* InSkeletalMesh);

private:
    TMap<FWString, std::shared_ptr<FTexture>> textureMap;
    TMap<FName, TWeakObjectPtr<UStaticMeshTest>> StaticMeshMap;
    TMap<FName, TWeakObjectPtr<USkeletalMesh>> SkeletalMeshMap;
};
