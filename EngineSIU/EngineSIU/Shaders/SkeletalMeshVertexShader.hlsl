#include "ShaderRegisters.hlsl"

#ifdef LIGHTING_MODEL_GOURAUD
SamplerState DiffuseSampler : register(s0);

Texture2D DiffuseTexture : register(t0);

cbuffer MaterialConstants : register(b1)
{
    FMaterial Material;
}

#include "Light.hlsl"
#endif

cbuffer BoneMatrices : register(b11)
{
    row_major matrix BoneMatrixArray[64]; // Maximum of 128 bones, adjust as needed
}

struct VS_INPUT_SkeletalMesh
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 UV : TEXCOORD;
    uint MaterialIndex : MATERIAL_INDEX;
    int4 BoneIndices : BONE_INDICES;
    float4 BoneWeights : BONE_WEIGHTS;
};

struct PS_INPUT_SkeletalMesh
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float4 WorldTangent : TEXCOORD2;
    float3 WorldPosition : TEXCOORD3;
    nointerpolation uint MaterialIndex : MATERIAL_INDEX;
};

PS_INPUT_SkeletalMesh mainVS(VS_INPUT_SkeletalMesh Input, uint id: SV_VertexID)
{
    PS_INPUT_SkeletalMesh Output;

    // Skinning: Transform vertex position and normal with bone matrices
    float4 SkinnedPosition = float4(0.0, 0.0, 0.0, 0.0);
    float3 SkinnedNormal = float3(0.0, 0.0, 0.0);
    float4 SkinnedTangent = float4(0.0, 0.0, 0.0, 0.0);
    
    for (int i = 0; i < 4; i++)
    {
        if (Input.BoneWeights[i] > 0.0f)
        {
            matrix BoneMatrix = BoneMatrixArray[Input.BoneIndices[i]];
            BoneMatrix = float4x4(  1, 0, 0, 0,
                                    0, 1, 0, 0,
                                    0, 0, 1, 0, 
                                    0, 0, 0, 1
                                    );
            //SkinnedPosition += mul(float4(Input.Position, 1.0), BoneMatrix) * Input.BoneWeights[i];
            SkinnedPosition += mul(float4(Input.Position, 1.0), BoneMatrix) * Input.BoneWeights[i];
            SkinnedNormal += mul(Input.Normal, (float3x3) BoneMatrix) * Input.BoneWeights[i];
            SkinnedTangent.xyz += mul(Input.Tangent.xyz, (float3x3) BoneMatrix) * Input.BoneWeights[i];
        }
    }
    
    SkinnedPosition = float4(Input.Position, 1) + float4(0, 0, 0, 0);

    SkinnedNormal = normalize(SkinnedNormal);
    SkinnedTangent.xyz = normalize(SkinnedTangent.xyz);
    SkinnedTangent.w = Input.Tangent.w;

    // Transform position to world space
    Output.Position = SkinnedPosition;
    Output.Position = mul(Output.Position, WorldMatrix);
    Output.WorldPosition = Output.Position.xyz;

    // Transform position to view-projection space
    Output.Position = mul(Output.Position, ViewMatrix);
    Output.Position = mul(Output.Position, ProjectionMatrix);

    // Transform normal and tangent to world space
    Output.WorldNormal = mul(SkinnedNormal, (float3x3) InverseTransposedWorld);
    Output.WorldTangent = float4(mul(SkinnedTangent.xyz, (float3x3) WorldMatrix), SkinnedTangent.w);

    Output.UV = Input.UV;
    Output.MaterialIndex = Input.MaterialIndex;

#ifdef LIGHTING_MODEL_GOURAUD
    float3 DiffuseColor = Input.Color.rgb;
    if (Material.TextureFlag & TEXTURE_FLAG_DIFFUSE)
    {
        DiffuseColor = DiffuseTexture.SampleLevel(DiffuseSampler, Input.UV, 0).rgb;
    }
    float3 Diffuse = Lighting(Output.WorldPosition, Output.WorldNormal, ViewWorldLocation, DiffuseColor, Material.SpecularColor, Material.Shininess);
    Output.Color = float4(Diffuse.rgb, 1.0);
#else
    Output.Color = Input.Color;
#endif
    return Output;
}
