#pragma once
#define _TCHAR_DEFINED
#include <d3d11.h>
#include "Math/Matrix.h"
#include "Container/Set.h"
#include "Container/Map.h"
#include "Math/Color.h"
#include "Engine/Classes/Engine/Texture.h"
#include "IRenderPass.h"

class FDXDBufferManager;
class FGraphicsDevice;
class UWorld;
class FEditorViewportClient;
class FDXDShaderManager;
class FEditorRenderPass : public IRenderPass
{
    // 클래스 내부에서만 사용하는 것들
private:
    enum class IconType
    {
        None,
        DirectionalLight,
        PointLight,
        SpotLight,
        AmbientLight,
        ExponentialFog,
        AtmosphericFog,
    };

    struct FRenderResourcesDebug
    {
        struct FWorldComponentContainer
        {
            TArray<class UStaticMeshComponent*> StaticMesh;
            TArray<class USkeletalMeshComponent*> SkinnedMesh;
            TArray<class UDirectionalLightComponent*> DirLight;
            TArray<class USpotLightComponent*> SpotLight;
            TArray<class UPointLightComponent*> PointLight;
            TArray<class UAmbientLightComponent*> AmbientLight;
            TArray<class UHeightFogComponent*> Fog;
            TArray<class UBoxComponent*> Box;
            TArray<class USphereComponent*> Sphere;
            TArray<class UCapsuleComponent*> Capsule;
        } Components;

        TMap<IconType, std::shared_ptr<FTexture>> IconTextures;
    };

public:
    virtual ~FEditorRenderPass() {}
    
    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;
    
    virtual void PrepareRenderArr() override;

    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    virtual void ClearRenderArr() override;

    void Release();

    void ReloadShader();

private:
    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
    FDXDShaderManager* ShaderManager;
    FRenderResourcesDebug Resources;

    void CreateShaders();

    void ReleaseShaders();

    void CreateBuffers();
    void CreateConstantBuffers();

    void LazyLoad();

    void PrepareRendertarget(const std::shared_ptr<FEditorViewportClient>& Viewport);

    // Gizmo 관련 함수
    //void RenderGizmos(const UWorld* World);
    //void PrepareShaderGizmo();
    //void PrepareConstantbufferGizmo();

    // Axis
    const FWString AxisKeyW = L"DebugAxis";
    const FString AxisKey = "DebugAxis";
    void RenderAxis();

    // AABB
    struct FConstantBufferDebugAABB
    {
        FVector Position;
        float Padding1;

        FVector Extent;
        float Padding2;
    };
    const FWString AABBKeyW = L"DebugAABB";
    const FString AABBKey = "DebugAABB";
    void RenderAABBInstanced();

    // Sphere
    struct alignas(16) FConstantBufferDebugSphere
    {
        FVector Position;
        float Radius;
        FLinearColor Color;
    };
    const FWString SphereKeyW = L"DebugSphere";
    const FString SphereKey = "DebugSphere";
    void RenderPointlightInstanced();

    //// Cone
    struct alignas(16) FConstantBufferDebugCone
    {
        alignas(16) FVector ApexPosiiton;
        float Radius;
        FVector Direction;
        float Angle;
        FLinearColor Color;
    };
    const uint32 NumConeSegments = 16;
    const FWString ConeKeyW = L"DebugCone";
    const FString ConeKey = "DebugCone"; // vertex / index 버퍼 없음
    void RenderSpotlightInstanced();

    // Grid
    struct alignas(16) FConstantBufferDebugGrid
    {
        FVector GridOrigin; // Grid의 중심
        float GridSpacing;
        int GridCount; // 총 grid 라인 수
        float Color;
        float Alpha; // 적용 안됨
        float padding;
    };
    const FWString GridKeyW = L"DebugGrid";
    const FString GridKey = "DebugGrid";
    void RenderGrid(const std::shared_ptr<FEditorViewportClient>& Viewport);

    //// Icon
    struct alignas(16) FConstantBufferDebugIcon
    {
        alignas(16) FVector Position;
        float Scale;
        FLinearColor Color;
    };
    const FWString IconKeyW = L"DebugIcon";
    const FString IconKey = "DebugIcon";
    void RenderIcons(const std::shared_ptr<FEditorViewportClient>& Viewport);

    //// Arrow
    struct FConstantBufferDebugArrow
    {
        alignas(16) FVector Position;
        float ArrowScaleXYZ;
        alignas(16) FVector Direction;
        float ArrowScaleZ;
        FLinearColor Color;
    };
    const FWString ArrowKeyW = L"DebugArrow";
    const FString ArrowKey = "DebugArrow";
    void RenderArrows();

    //// ShapeComponents
    struct FConstantBufferDebugOrientedBox
    {
        alignas(16) FVector Position;
        float pad0;
        alignas(16) FVector Rotation;
        float pad1;
        alignas(16) FVector Extent;
        float pad2;
        FLinearColor Color;
    };

    struct FConstantBufferDebugCapsule
    {
        alignas(16) FVector PointA;
        float Radius;
        alignas(16) FVector PointB;
        float pad;
        FLinearColor Color;
    };
    const FWString CapsuleKeyW = L"DebugCapsule";
    const FString CapsuleKey = "DebugCapsule";
    void RenderShapes();

    //// SkinnedMeshs
    struct FConstantBufferDebugPyramid
    {
        FVector Position;
        float Height = 1;
        FVector Direction;
        float BaseSize = 1;
        FLinearColor Color = FLinearColor::White;
    };
    const FWString PyramidKeyW = L"DebugPyramid";
    const FString PyramidKey = "DebugPyramid";
    void RenderSkinnedMeshs();

    const UINT32 ConstantBufferSizeAABB = 8;
    const UINT32 ConstantBufferSizeSphere = 32;
    const UINT32 ConstantBufferSizeCone = 16; // 최대
    const uint32 ConstantBufferSizeIcon = 16;
    const uint32 ConstantBufferSizeCapsule = 8;
    const uint32 ConstantBufferSizePyramid = 32;
};

