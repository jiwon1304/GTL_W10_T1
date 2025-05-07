#pragma once
#include "Container/Array.h"

class UMeshAsset;
class UStaticMeshTest;
class USkeletalMesh;


struct FSerializeMeshAsset
{
public:
    static constexpr uint32 Version = 1;

    static UStaticMeshTest* LoadStaticMeshFromBinary(const FString& FilePath);
    static USkeletalMesh* LoadSkeletalMeshFromBinary(const FString& FilePath);

    static bool SaveStaticMeshToBinary(const FString& FilePath, UStaticMeshTest* StaticMesh);
    static bool SaveSkeletalMeshToBinary(const FString& FilePath, USkeletalMesh* SkeletalMesh);

private:
    static bool SerializeVersion(FArchive& Ar);
    static FArchive& SerializeMeshAssetBase(FArchive& Ar, UMeshAsset* Mesh);
    static FArchive& SerializeMeshAsset(FArchive& Ar, UStaticMeshTest* StaticMesh);
    static FArchive& SerializeMeshAsset(FArchive& Ar, USkeletalMesh* SkeletalMesh);
};
