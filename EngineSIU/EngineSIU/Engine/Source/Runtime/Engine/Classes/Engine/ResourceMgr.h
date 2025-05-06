#pragma once
#include <memory>
#include "Texture.h"
#include "Container/Map.h"
#include "UObject/NameTypes.h"

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
    std::shared_ptr<UStaticMeshTest> GetStaticMesh(FName InName) const;
    std::shared_ptr<USkeletalMesh> GetSkeletalMesh(FName InName) const;

    void AddStaticMesh(FName InName, const std::shared_ptr<UStaticMeshTest>& InStaticMesh);
    void AddSkeletalMesh(FName InName, const std::shared_ptr<USkeletalMesh>& InSkeletalMesh);

private:
    TMap<FWString, std::shared_ptr<FTexture>> textureMap;
    TMap<FName, std::shared_ptr<UStaticMeshTest>> StaticMeshMap;
    TMap<FName, std::shared_ptr<USkeletalMesh>> SkeletalMeshMap;
};
