// SkinningCS.hlsl
// Compute Shader for GPU Skinning
// register(b0) : CBoneMatrices
// register(t0) : StructuredBuffer<FSkeletalVertex> InVerts
// register(u0) : RWStructuredBuffer<FSkeletalVertex> OutVerts

#define MAX_BONES 128

// 기존 본 매트릭스
cbuffer CBoneMatrices : register(b0)
{
    float4x4 BoneMatrices[MAX_BONES];
}

// 요소 개수를 넘겨줄 새로운 상수버퍼 (b1)
cbuffer CDispatchInfo : register(b1)
{
    uint NumVertices;
}

struct FSkeletalVertex
{
    float3 Position;
    float4 Color;
    float3 Normal;
    float4 Tangent;
    float2 UV;
    uint MaterialIndex;
    int4 BoneIndices0;
    int4 BoneIndices1;
    float4 BoneWeights0;
    float4 BoneWeights1;
};

StructuredBuffer<FSkeletalVertex> InVerts : register(t0);
RWStructuredBuffer<FSkeletalVertex> OutVerts : register(u0);

[numthreads(256, 1, 1)]
void mainCS(uint3 DTid : SV_DispatchThreadID)
{
    uint idx = DTid.x;
    if (idx >= NumVertices)
        return;

    FSkeletalVertex vin = InVerts[idx];
    float4 skPos = float4(0, 0, 0, 0);
    float4 skNrm = float4(0, 0, 0, 0);
    float4 skTan = float4(0, 0, 0, 0);

    // 첫 4개 인플루언스
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float w = vin.BoneWeights0[i];
        if (w > 0)
        {
            int bi = vin.BoneIndices0[i];
            skPos += mul(BoneMatrices[bi], float4(vin.Position, 1)) * w;
            skNrm += mul(BoneMatrices[bi], float4(vin.Normal, 0)) * w;
            skTan += mul(BoneMatrices[bi], float4(vin.Tangent)) * w;
        }
    }
    // 나머지 4개 인플루언스
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float w = vin.BoneWeights1[i];
        if (w > 0)
        {
            int bi = vin.BoneIndices1[i];
            skPos += mul(BoneMatrices[bi], float4(vin.Position, 1)) * w;
            skNrm += mul(BoneMatrices[bi], float4(vin.Normal, 0)) * w;
            skTan += mul(BoneMatrices[bi], float4(vin.Tangent)) * w;
        }
    }

    vin.Position = skPos.xyz;
    vin.Normal = normalize(skNrm.xyz);
    vin.Tangent = normalize(skTan.xyzw);

    OutVerts[idx] = vin;
}
