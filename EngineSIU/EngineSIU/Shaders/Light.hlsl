
#define MAX_LIGHTS 16 

#define MAX_DIRECTIONAL_LIGHT 16
#define MAX_POINT_LIGHT 16
#define MAX_SPOT_LIGHT 16
#define MAX_AMBIENT_LIGHT 16

#define POINT_LIGHT         1
#define SPOT_LIGHT          2
#define DIRECTIONAL_LIGHT   3
#define AMBIENT_LIGHT       4

#define MAX_LIGHT_PER_TILE 1024

#define NEAR_PLANE 0.1
#define LIGHT_RADIUS_WORLD 7

#define MAX_CASCADE_NUM 5

struct FAmbientLightInfo
{
    float4 AmbientColor;
};

struct FDirectionalLightInfo
{
    float4 LightColor;

    float3 Direction;
    float Intensity;
    
    row_major matrix LightViewProj;
    row_major matrix LightInvProj; // 섀도우맵 생성 시 사용한 VP 행렬
    
    uint ShadowMapArrayIndex;//캐스캐이드전 임시 배열
    uint  CastShadows;
    float ShadowBias;
    float Padding3; // 필요시

    float OrthoWidth;
    // 직교 투영 볼륨의 월드 단위 높이 (섀도우 영역)
    float OrthoHeight;
    // 섀도우 계산을 위한 라이트 시점의 Near Plane (음수 가능)
    float ShadowNearPlane;
    // 섀도우 계산을 위한 라이트 시점의 Far Plane
    float ShadowFarPlane;
};

struct FPointLightInfo
{
    float4 LightColor;

    float3 Position;
    float Radius;

    int Type;
    float Intensity;
    float Attenuation;
    float Padding;

    // --- Shadow Info ---
    row_major matrix LightViewProj[6]; // 섀도우맵 생성 시 사용한 VP 행렬
    
    uint CastShadows;
    float ShadowBias;
    uint ShadowMapArrayIndex; // 필요시
    float Padding2; // 필요시
};

struct FSpotLightInfo
{
    float4 LightColor;

    float3 Position;
    float Radius;

    float3 Direction;
    float Intensity;

    int Type;
    float InnerRad;
    float OuterRad;
    float Attenuation;
    
    // --- Shadow Info ---
    row_major matrix LightViewProj; // 섀도우맵 생성 시 사용한 VP 행렬
    
    uint CastShadows;
    float ShadowBias;
    uint ShadowMapArrayIndex; // 필요시
    float Padding2; // 필요시
};

cbuffer FLightInfoBuffer : register(b0)
{
    FAmbientLightInfo Ambient[MAX_AMBIENT_LIGHT];
    FDirectionalLightInfo Directional[MAX_DIRECTIONAL_LIGHT];
    FPointLightInfo PointLights[MAX_POINT_LIGHT]; //삭제 예정
    FSpotLightInfo SpotLights[MAX_SPOT_LIGHT]; // 삭제 예정
    
    int DirectionalLightsCount;
    int PointLightsCount;
    int SpotLightsCount;
    int AmbientLightsCount;
};

cbuffer ShadowFlagConstants : register(b5)
{
    bool IsShadow;
    float3 shadowFlagPad0;
}

cbuffer TileLightCullSettings : register(b8)
{
    uint2 ScreenSize; // 화면 해상도
    uint2 TileSize; // 한 타일의 크기 (예: 16x16)

    float NearZ; // 카메라 near plane
    float FarZ; // 카메라 far plane

    row_major matrix TileViewMatrix; // View 행렬
    row_major matrix TileProjectionMatrix; // Projection 행렬
    row_major matrix TileInverseProjection; // Projection^-1, 뷰스페이스 복원용

    uint NumLights; // 총 라이트 수
    uint Enable25DCulling; // 1이면 2.5D 컬링 사용
}

cbuffer CascadeConstantBuffer : register(b9)
{
    row_major matrix World;
    row_major matrix CascadedViewProj[MAX_CASCADE_NUM];
    row_major matrix CascadedInvViewProj[MAX_CASCADE_NUM];
    row_major matrix CascadedInvProj[MAX_CASCADE_NUM];
    float4 CascadeSplits;
    float2 cascadepad;
};

struct LightPerTiles
{
    uint NumLights;
    uint Indices[MAX_LIGHT_PER_TILE];
    uint Padding[3];
};

// 인덱스별 색 지정 함수
float4 DebugCSMColor(uint idx)
{
    if (idx == 0)
        return float4(1, 0, 0, 1); // 빨강
    if (idx == 1)
        return float4(0, 1, 0, 1); // 초록
    if (idx == 2)
        return float4(0, 0, 1, 1); // 파랑
    return float4(1, 1, 1, 1); // 나머지 – 흰색
}

StructuredBuffer<FPointLightInfo> gPointLights : register(t10);
StructuredBuffer<FSpotLightInfo> gSpotLights   : register(t11);

StructuredBuffer<uint> PerTilePointLightIndexBuffer : register(t12);
StructuredBuffer<uint> PerTileSpotLightIndexBuffer  : register(t13);



SamplerComparisonState ShadowSamplerCmp : register(s10);
SamplerState ShadowPointSampler : register(s11);

Texture2DArray SpotShadowMapArray : register(t50);
Texture2DArray DirectionShadowMapArray : register(t51);
TextureCubeArray PointShadowMapArray : register(t52);

bool InRange(float val, float min, float max)
{
    return (min <= val && val <= max);
}

uint GetCascadeIndex(float viewDepth)
{
    // viewDepth 는 LightSpace 깊이(z) 또는 NDC 깊이 복원 뷰 깊이

    for (uint i = 0; i < MAX_CASCADE_NUM; ++i)
    {
        // splits 배열에는 [0]=near, [N]=far 까지 로그 스플릿 저장됨 ex)0..2..4..46..1000
        if (viewDepth <= CascadeSplits[i + 1])
            return i;
    }
    return MAX_CASCADE_NUM - 1;
}

// NdcDepthToViewDepth
float N2V(float ndcDepth, matrix invProj)
{
    float4 pointView = mul(float4(0, 0, ndcDepth, 1), invProj);
    return pointView.z / pointView.w;
}



float PCF_Filter(float2 uv, float zReceiverNdc, float filterRadiusUV, uint csmIndex)
{
    float sum = 0.0f;
    [unroll]
    for (int i = 0; i < 64; ++i)
    {
        // TODO (offset, 0)에 배열(slice) 인덱스 넣기
        float2 offset = diskSamples64[i] * filterRadiusUV;
        sum += DirectionShadowMapArray.SampleCmpLevelZero(
            ShadowSamplerCmp, float3(uv + offset, csmIndex), zReceiverNdc);
    }
    return sum / 64;
}


void FindBlocker(out float avgBlockerDepthView, out float numBlockers, float2 uv,
                 float zReceiverView, Texture2DArray DirectionShadowMapArray, matrix InvProj, float LightRadiusWorld, uint csmIndex, FDirectionalLightInfo LightInfo)

{
    float LightRadiusUV = LightRadiusWorld / LightInfo.OrthoWidth; // TO FIX!
    float searchRadius = LightRadiusUV * (zReceiverView - NEAR_PLANE) / zReceiverView; // TO FIX! NearPlane
    float blockerSum = 0;
    numBlockers = 0;

    for (int i = 0; i < 64; ++i)
    {
        // TODO : slice index 받아야 함 (searchRadius , 배열 인덱스)
        float ShadowMapDepth = DirectionShadowMapArray.SampleLevel(ShadowPointSampler, float3(uv + diskSamples64[i] * searchRadius, csmIndex), 0).r;
        ShadowMapDepth = N2V(ShadowMapDepth, InvProj);
        if (ShadowMapDepth < zReceiverView)
        {
            blockerSum += ShadowMapDepth;
            numBlockers += 1;
        }
    }
    avgBlockerDepthView = (numBlockers > 0) ? (blockerSum / numBlockers) : 0.0f;
}


float PCSS(float2 uv, float zReceiverNDC, Texture2DArray DirectionShadowMapArray, matrix ShadowInvProj, float LightRadiusWorld, uint csmIndex, FDirectionalLightInfo LightInfo)
{
    float lightRadiusUV = LightRadiusWorld / 2.0; // TO FIX!
    float zReceiverView = N2V(zReceiverNDC, CascadedInvProj[csmIndex]);


    // 1. Blocker Search
    float avgBlockerDepthView = 0;
    float numBlockers = 0;

    FindBlocker(avgBlockerDepthView, numBlockers, uv, zReceiverView, DirectionShadowMapArray, CascadedInvProj[csmIndex], LightRadiusWorld, csmIndex, LightInfo);


    if (numBlockers < 1)
    {
        // There are no Occluders so early out(this saves filtering)
        return 1.0f;
    }
    else
    {
        // 2. Penumbra Size
        float penumbraRatio = (zReceiverView - avgBlockerDepthView) / avgBlockerDepthView;
        float filterRadiusUV = penumbraRatio * lightRadiusUV * NEAR_PLANE / zReceiverView; // TO FIX!!!!

        // 3. Filtering
        return PCF_Filter(uv, zReceiverNDC, filterRadiusUV, csmIndex);
    }
}


float CalculateDirectionalShadowFactor(float3 WorldPosition, float3 WorldNormal, FDirectionalLightInfo LightInfo, // 라이트 정보 전체 전달
                                Texture2DArray DirectionShadowMapArray,
                                SamplerComparisonState ShadowSampler)
{
    if (LightInfo.CastShadows == false)
    {
        return 1.0f;
    }
    
    float ShadowFactor = 1.0;
    float NdotL = dot(normalize(WorldNormal), LightInfo.Direction);
    float bias = 0.01f;
    
    // 1. Project World Position to Light Screen Space
    float4 LightScreen = mul(float4(WorldPosition, 1.0f), LightInfo.LightViewProj);
    LightScreen.xyz /= LightScreen.w; // Perspective Divide -> [-1, 1] 범위로 변환

    // 2. 광원 입장의 Texture 좌표계로 변환
    float2 ShadowMapTexCoord = { LightScreen.x, -LightScreen.y }; // NDC 좌표계와 UV 좌표계는 Y축 방향이 반대
    ShadowMapTexCoord += 1.0;
    ShadowMapTexCoord *= 0.5;

    float LightDistance = LightScreen.z;
    LightDistance -= bias;


    float4 posCam = mul(float4(WorldPosition, 1), ViewMatrix);
    float depthCam = posCam.z; 
    uint CsmIndex = GetCascadeIndex(depthCam);
    //CsmIndex = 1;
    float4 posLS = mul(float4(WorldPosition, 1), CascadedViewProj[CsmIndex]);
    float2 uv = posLS.xy * 0.5f + 0.5f;
    uv.y = 1 - uv.y;
    float zReceiverNdc = posLS.z -= bias;
    ShadowFactor = DirectionShadowMapArray.SampleCmpLevelZero(ShadowSamplerCmp, float3(uv, CsmIndex), zReceiverNdc);

    //ShadowFactor = PCSS(uv, zReceiverNdc, DirectionShadowMapArray, CascadedInvViewProj[CsmIndex], LIGHT_RADIUS_WORLD, CsmIndex, LightInfo);
    return ShadowFactor;
    
    
}

int GetMajorFaceIndex(float3 Dir){
    float3 absDir = abs(Dir);
    if(absDir.x > absDir.y && absDir.x > absDir.z)
    {
        return Dir.x > 0.0f ? 0 : 1;
    }
    else if(absDir.y > absDir.z)
    {
        return Dir.y > 0.0f ? 2 : 3;
    }
    else
    {
        return Dir.z > 0.0f ? 4 : 5;
    }

}

float CalculatePointShadowFactor(float3 WorldPosition, FPointLightInfo LightInfo, // 라이트 정보 전체 전달
                                TextureCubeArray ShadowMapArray,
                                SamplerComparisonState ShadowSampler)
{
    // 1) 광원→조각 방향 (큐브맵 샘플링 좌표)
    float3 Dir = normalize(WorldPosition - LightInfo.Position);
    // 2) 해당 face의 뷰·프로젝션 적용
    int face = GetMajorFaceIndex(Dir);
    float4 posCS = mul(float4(WorldPosition, 1.0f), LightInfo.LightViewProj[face]);
    // 3) 클립스페이스 깊이
    float refDepth = posCS.z / posCS.w;
    // 5) 하드웨어 비교 샘플
    float3 SampleDir = normalize(WorldPosition - LightInfo.Position);
    float shadow = ShadowMapArray.SampleCmpLevelZero(ShadowSampler, float4(SampleDir, (float)LightInfo.ShadowMapArrayIndex), refDepth - LightInfo.ShadowBias).r;
    return shadow;
}


// cbuffer PointLightShadowConstants : register(b5)
// {
//     row_major matrix PointLightViewProj[NUM_FACES]; // 6 : NUM_FACES
//     float3 PointLightPos;
//     float pad0;
// }


// 기본적인 그림자 계산 함수 (Directional/Spot 용)
// 하드웨어 PCF (SamplerComparisonState 사용) 예시
float CalculateSpotShadowFactor(float3 WorldPosition, FSpotLightInfo LightInfo, // 라이트 정보 전체 전달
                                Texture2DArray ShadowMapArray,
                                SamplerComparisonState ShadowSampler)
{
    // if (!LightInfo.CastShadows)
    // {
    //     return 1.0f; // 그림자 안 드리움
    // }

    // 1 & 2. 라이트 클립 공간 좌표 계산
    float4 PixelPosLightClip = mul(float4(WorldPosition, 1.0f), LightInfo.LightViewProj);

    // 3. 섀도우 맵 UV 계산 [0, 1]
    float2 ShadowMapUV = PixelPosLightClip.xy / PixelPosLightClip.w;
    ShadowMapUV = ShadowMapUV * float2(0.5, -0.5) + 0.5; // Y 반전 필요시

    // 4. 현재 깊이 계산
    float CurrentDepth = PixelPosLightClip.z / PixelPosLightClip.w;

    // UV 범위 체크 (라이트 범위 밖)
    if (any(ShadowMapUV < 0.0f) || any(ShadowMapUV > 1.0f))
    {
        return 1.0f;
    }

    // 5 & 6. 배열의 특정 슬라이스 샘플링 및 비교
    float ShadowFactor = ShadowMapArray.SampleCmpLevelZero(
        ShadowSampler,
        float3(ShadowMapUV, (float)LightInfo.ShadowMapArrayIndex), // UV와 배열 인덱스 사용
        CurrentDepth - LightInfo.ShadowBias  // 바이어스 적용
    );

    return ShadowFactor;
}

float GetDistanceAttenuation(float Distance, float Radius)
{
    float  InvRadius = 1.0 / Radius;
    float  DistSqr = Distance * Distance;
    float  RadiusMask = saturate(1.0 - DistSqr * InvRadius * InvRadius);
    RadiusMask *= RadiusMask;
    
    return RadiusMask / (DistSqr + 1.0);
}

float GetSpotLightAttenuation(float Distance, float Radius, float3 LightDir, float3 SpotDir, float InnerRadius, float OuterRadius)
{
    float DistAtten = GetDistanceAttenuation(Distance, Radius);
    
    float  CosTheta = dot(SpotDir, -LightDir);
    float  SpotMask = saturate((CosTheta - cos(OuterRadius)) / (cos(InnerRadius) - cos(OuterRadius)));
    SpotMask *= SpotMask;
    
    return DistAtten * SpotMask;
}

float CalculateDiffuse(float3 WorldNormal, float3 LightDir)
{
    return max(dot(WorldNormal, LightDir), 0.0);
}

float CalculateSpecular(float3 WorldNormal, float3 ToLightDir, float3 ViewDir, float Shininess, float SpecularStrength = 0.5)
{
#ifdef LIGHTING_MODEL_GOURAUD
    float3 ReflectDir = reflect(-ToLightDir, WorldNormal);
    float Spec = pow(max(dot(ViewDir, ReflectDir), 0.0), Shininess);
#else
    float3 HalfDir = normalize(ToLightDir + ViewDir); // Blinn-Phong
    float Spec = pow(max(dot(WorldNormal, HalfDir), 0.0), Shininess);
#endif
    return Spec * SpecularStrength; 
}

float3 PointLight(int Index, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Shininess)
{
    FPointLightInfo LightInfo = gPointLights[Index];
    //FPointLightInfo LightInfo = PointLights[Index];
    
    float3 ToLight = LightInfo.Position - WorldPosition;
    float Distance = length(ToLight);
    
    float Attenuation = GetDistanceAttenuation(Distance, LightInfo.Radius);
    if (Attenuation <= 0.0)
    {
        return float4(0.f, 0.f, 0.f, 0.f);
    }
    
    // --- 그림자 계산
    float Shadow = 1.0;
    if (LightInfo.CastShadows && IsShadow)
    {
        // 그림자 계산
        Shadow = CalculatePointShadowFactor(WorldPosition, LightInfo, PointShadowMapArray, ShadowSamplerCmp);
        // 그림자 계수가 0 이하면 더 이상 계산 불필요
        if (Shadow <= 0.0)
        {
            return float3(0.0, 0.0, 0.0);
        }
    }
    
    float3 LightDir = normalize(ToLight);
    float DiffuseFactor = CalculateDiffuse(WorldNormal, LightDir);

    float3 Lit = (DiffuseFactor * DiffuseColor);
#ifndef LIGHTING_MODEL_LAMBERT
    float3 ViewDir = normalize(WorldViewPosition - WorldPosition);
    float SpecularFactor = CalculateSpecular(WorldNormal, LightDir, ViewDir, Shininess);
    Lit += SpecularFactor * SpecularColor;
#endif
    
    return Lit * Attenuation * LightInfo.Intensity * LightInfo.LightColor * Shadow;
}

float3 SpotLight(int Index, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Shininess)
{
    FSpotLightInfo LightInfo = gSpotLights[Index];
    // FSpotLightInfo LightInfo = SpotLights[Index];
    
    float3 ToLight = LightInfo.Position - WorldPosition;
    float Distance = length(ToLight);
    float3 LightDir = normalize(ToLight);
    
    float SpotlightFactor = GetSpotLightAttenuation(Distance, LightInfo.Radius, LightDir, normalize(LightInfo.Direction), LightInfo.InnerRad, LightInfo.OuterRad);
    if (SpotlightFactor <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }

    // --- 그림자 계산
    float Shadow = 1.0;
    if (LightInfo.CastShadows && IsShadow)
    {
        // 그림자 계산
        Shadow  = CalculateSpotShadowFactor(WorldPosition, LightInfo, SpotShadowMapArray, ShadowSamplerCmp);
        // 그림자 계수가 0 이하면 더 이상 계산 불필요
        if (Shadow <= 0.0)
        {
            return float3(0.0, 0.0, 0.0);
        }
    }

    float DiffuseFactor = CalculateDiffuse(WorldNormal, LightDir);
    
    float3 Lit = DiffuseFactor * DiffuseColor;
#ifndef LIGHTING_MODEL_LAMBERT
    float3 ViewDir = normalize(WorldViewPosition - WorldPosition);
    float SpecularFactor = CalculateSpecular(WorldNormal, LightDir, ViewDir, Shininess);
    Lit += SpecularFactor * SpecularColor;
#endif
    
    return Lit * SpotlightFactor * LightInfo.Intensity * LightInfo.LightColor * Shadow;
}

float3 DirectionalLight(int nIndex, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Shininess)
{
    FDirectionalLightInfo LightInfo = Directional[nIndex];
    
    float3 LightDir = normalize(-LightInfo.Direction);
    float3 ViewDir = normalize(WorldViewPosition - WorldPosition);
    float DiffuseFactor = CalculateDiffuse(WorldNormal, LightDir);

    float4 posCam = mul(float4(WorldPosition, 1), ViewMatrix);
    float depthCam = posCam.z / posCam.w;
    uint csmIndex = GetCascadeIndex(depthCam); // 시각적 디버깅용
    
    // --- 그림자 계산
    float Shadow = 1.0;
    if (IsShadow)
    {
        Shadow = CalculateDirectionalShadowFactor(WorldPosition, WorldNormal, LightInfo, DirectionShadowMapArray, ShadowSamplerCmp);
        // 그림자 계수가 0 이하면 더 이상 계산 불필요
        if (Shadow <= 0.0)
        {
            return float4(0.0, 0.0, 0.0, 0.0);
        }
    }

    float3 Lit = DiffuseFactor * DiffuseColor;
#ifndef LIGHTING_MODEL_LAMBERT
    float SpecularFactor = CalculateSpecular(WorldNormal, LightDir, ViewDir, Shininess);
    Lit += SpecularFactor * SpecularColor;
#endif
    return Lit * LightInfo.Intensity * LightInfo.LightColor * Shadow; /** DebugCSMColor(csmIndex)*/;
}

float4 Lighting(float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Shininess, uint TileIndex)
{
    float3 FinalColor = float3(0.0, 0.0, 0.0);

    int BucketsPerTile = MAX_LIGHT_PER_TILE / 32;
    int StartIndex = TileIndex * BucketsPerTile;
    for (int Bucket = 0; Bucket < BucketsPerTile; ++Bucket)
    {
        int PointMask = PerTilePointLightIndexBuffer[StartIndex + Bucket];
        int SpotMask = PerTileSpotLightIndexBuffer[StartIndex + Bucket];
        for (int bit = 0; bit < 32; ++bit)
        {
            if (PointMask & (1u << bit))
            {
                // 전역 조명 인덱스는 bucket * 32 + bit 로 계산됨.
                // 전역 조명 인덱스가 총 조명 수보다 작은 경우에만 추가
                int GlobalPointLightIndex = Bucket * 32 + bit;
                if (GlobalPointLightIndex < MAX_LIGHT_PER_TILE)
                {
                    FinalColor += PointLight(GlobalPointLightIndex, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);

                }
            }
            if (SpotMask & (1u << bit))
            {
                int GlobalSpotLightIndex = Bucket * 32 + bit;
                if (GlobalSpotLightIndex < MAX_LIGHT_PER_TILE)
                {
                    FinalColor += SpotLight(GlobalSpotLightIndex, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);
                }
            }
        }
    }
    
    // [unroll(MAX_SPOT_LIGHT)]
    // for (int j = 0; j < SpotLightsCount; j++)
    // {
    //     FinalColor += SpotLight(j, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor);
    // }
    [unroll(MAX_DIRECTIONAL_LIGHT)]
    for (int k = 0; k < 1; k++)
    {
        FinalColor += DirectionalLight(k, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);
    }
    [unroll(MAX_AMBIENT_LIGHT)]
    for (int l = 0; l < AmbientLightsCount; l++) 
    {
        FinalColor += float4(Ambient[l].AmbientColor.rgb * DiffuseColor, 0.0);
    }
    
    return float4(FinalColor, 1.0);
}

float4 Lighting(float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Shininess)
{
    float3 FinalColor = float3(0.0, 0.0, 0.0);

    // 다소 비효율적일 수도 있음.
    [unroll(MAX_POINT_LIGHT)]
    for (int i = 0; i < PointLightsCount; i++)
    {
        FinalColor += PointLight(i, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);
    }

    [unroll(MAX_SPOT_LIGHT)]
    for (int j = 0; j < SpotLightsCount; j++)
    {
        FinalColor += SpotLight(j, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);
    }
    [unroll(MAX_DIRECTIONAL_LIGHT)]
    for (int k = 0; k < 1; k++) 
    {
        FinalColor += DirectionalLight(k, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);
    }
    [unroll(MAX_AMBIENT_LIGHT)]
    for (int l = 0; l < AmbientLightsCount; l++)
    {
        FinalColor += float4(Ambient[l].AmbientColor.rgb * DiffuseColor, 0.0);
    }
    
    return float4(FinalColor, 1.0);
}



