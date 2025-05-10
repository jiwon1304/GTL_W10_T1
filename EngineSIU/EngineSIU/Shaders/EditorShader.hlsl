
#include "EditorShaderConstants.hlsli"
 //#include "ShaderConstants.hlsli"

#include "ShaderRegisters.hlsl"

cbuffer MaterialConstants : register(b1)
{
    FMaterial Material;
}

// Input Layout은 float3이지만, shader에서 missing w = 1로 처리해서 사용
// https://stackoverflow.com/questions/29728349/hlsl-sv-position-why-how-from-float3-to-float4
struct VS_INPUT
{
    float4 position : POSITION; // 버텍스 위치
    float4 color : COLOR; // 버텍스 색상
    float3 normal : NORMAL; // 버텍스 노멀
    float2 texcoord : TEXCOORD;
    int materialIndex : MATERIAL_INDEX;
};

struct PS_INPUT
{
    float4 position : SV_Position;
    float4 color : COLOR;
};

/////////////////////////////////////////////
// GIZMO
PS_INPUT gizmoVS(VS_INPUT input)
{
    PS_INPUT output;
    
    float4 pos;
    pos = mul(input.position, WorldMatrix);
    pos = mul(pos, ViewMatrix);
    pos = mul(pos, ProjectionMatrix);
    
    output.position = pos;
    
    output.color = float4(Material.DiffuseColor, 1.f);
    
    return output;
}

float4 gizmoPS(PS_INPUT input) : SV_Target
{
    return input.color;
}

/////////////////////////////////////////////
// Axis
// Input buffer는 없고 대신 Draw(6)하면됨.

const static float4 AxisPos[6] =
{
    float4(0, 0, 0, 1),
    float4(10000000, 0, 0, 1),
    float4(0, 0, 0, 1),
    float4(0, 10000000, 0, 1),
    float4(0, 0, 0, 1),
    float4(0, 0, 10000000, 1)
};

const static float4 AxisColor[3] =
{
    float4(1, 0, 0, 1),
    float4(0, 1, 0, 1),
    float4(0, 0, 1, 1)
};

struct PS_INPUT_AXIS
{
    float4 position : SV_Position;
    float4 worldPos : POSITION;
    float4 color : COLOR;
};

// Draw()에서 NumVertices만큼 SV_VertexID만 다른채로 호출됨.
// 어차피 월드에 하나이므로 Vertex를 받지않음.
PS_INPUT_AXIS axisVS(uint vertexID : SV_VertexID)
{
    PS_INPUT_AXIS output;
    
    float4 Vertex = AxisPos[vertexID];
    output.worldPos = Vertex;
    Vertex = mul(Vertex, ViewMatrix);
    Vertex = mul(Vertex, ProjectionMatrix);
    output.position = Vertex;
    
    output.color = AxisColor[vertexID / 2];
    return output;
}

float4 axisPS(PS_INPUT_AXIS input) : SV_Target
{
    float Dist = length(input.worldPos.xyz - ViewWorldLocation);

    float MaxDist = 400 * 1.2f;
    float MinDist = MaxDist * 0.3f;

    // Fade out grid
    float Fade = saturate(1.f - (Dist - MinDist) / (MaxDist - MinDist));
    input.color.a *= Fade * Fade * Fade;
    
    return input.color;
}

/////////////////////////////////////////////
// AABB
struct VS_INPUT_POS_ONLY
{
    float4 position : POSITION0;
};

PS_INPUT aabbVS(VS_INPUT_POS_ONLY input, uint instanceID : SV_InstanceID)
{
    PS_INPUT output;
    
    float3 pos = DataAABB[instanceID].Position;
    float3 scale = DataAABB[instanceID].Extent;
    //scale = float3(1, 1, 1);
    
    float4 localPos = float4(input.position.xyz * scale + pos, 1.f);
        
    localPos = mul(localPos, ViewMatrix);
    localPos = mul(localPos, ProjectionMatrix);
    output.position = localPos;
    
    // color는 지정안해줌
    
    return output;
}

float4 aabbPS(PS_INPUT input) : SV_Target
{
    return float4(1.0f, 1.0f, 0.0f, 1.0f); // 노란색 AABB
}

/////////////////////////////////////////////
// Sphere
PS_INPUT sphereVS(VS_INPUT_POS_ONLY input, uint instanceID : SV_InstanceID)
{
    PS_INPUT output;
    
    float3 pos = DataSphere[instanceID].Position;
    float scale = DataSphere[instanceID].Radius;
    //scale = float3(1, 1, 1);
    
    float4 localPos = float4(input.position.xyz * scale + pos, 1.f);
        
    localPos = mul(localPos, ViewMatrix);
    localPos = mul(localPos, ProjectionMatrix);
    output.position = localPos;
    output.color = DataSphere[instanceID].Color;
    // color는 지정안해줌
    
    return output;
}

float4 spherePS(PS_INPUT input) : SV_Target
{
    return input.color; // 하늘색
}

/////////////////////////////////////////////
// Cone
float3x3 CreateRotationMatrixFromX(float3 targetDir)
{
    float3 from = float3(1, 0, 0); // 기준 방향 X축
    float3 to = normalize(targetDir); // 타겟 방향 정규화

    float cosTheta = dot(from, to);

    // 이미 정렬된 경우: 단위 행렬 반환
    if (cosTheta > 0.9999f)
    {
        return float3x3(
            1, 0, 0,
            0, 1, 0,
            0, 0, 1
        );
    }

    // 반대 방향인 경우: 180도 회전, 축은 Y축이나 Z축 아무거나 가능
    if (cosTheta < -0.9999f)
    {
        float3 up = float3(0.0f, 1.0f, 0.0f);
        float3 axis = normalize(cross(from, up));
        float x = axis.x, y = axis.y, z = axis.z;
        float3x3 rot180 = float3x3(
            -1 + 2 * x * x, 2 * x * y, 2 * x * z,
                2 * x * y, -1 + 2 * y * y, 2 * y * z,
                2 * x * z, 2 * y * z, -1 + 2 * z * z
        );
        return rot180;
    }

    // 일반적인 경우: Rodrigues' rotation formula
    float3 axis = normalize(cross(to, from)); // 왼손 좌표계 보정
    float s = sqrt(1.0f - cosTheta * cosTheta); // sin(theta)
    float3x3 K = float3x3(
         0, -axis.z, axis.y,
         axis.z, 0, -axis.x,
        -axis.y, axis.x, 0
    );

    float3x3 I = float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1);
    float3x3 R = I + s * K + (1 - cosTheta) * mul(K, K);
    return R;
}
PS_INPUT coneVS(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    const int NUM_SEGMENTS = 16;
    PS_INPUT output;
    
    int SegmentIndex;
    // cone의 옆면
    float Angle = DataCone[instanceID].Angle;
    float TangentAngle = tan(Angle);
    float SinAngle = sin(Angle);
    float CosAngle = cos(Angle);
    float radius = DataCone[instanceID].Radius;
    
    float3 localPos3;
    // 원뿔의 빗면
    if (vertexID == 0)
    {
        localPos3 = float3(0, 0, 0);
    }
    else if (vertexID < NUM_SEGMENTS + 1)
    {
        float ConeBaseRadius = radius * SinAngle;
        float ConeHeight = radius * CosAngle;
        SegmentIndex = (vertexID - 1);
        float SegmentAngle = SegmentIndex / (float) NUM_SEGMENTS * 2.0f * 3.1415926535897932f;
        localPos3 = float3(ConeHeight, ConeBaseRadius, ConeBaseRadius);
        localPos3 = localPos3 * float3(1.f, cos(SegmentAngle), sin(SegmentAngle));
    }
    // xz plane의 구
    else if (vertexID < NUM_SEGMENTS + 1 + NUM_SEGMENTS + 1)
    {
        SegmentIndex = (vertexID - (NUM_SEGMENTS + 1));
        float SegmentAngle = SegmentIndex / (float) (NUM_SEGMENTS) * (2 * Angle);
        float angleOffset = -Angle;
        localPos3 = float3(cos(angleOffset + SegmentAngle), 0, sin(angleOffset + SegmentAngle));
        localPos3 = localPos3 * float3(radius, radius, radius) * 1;
    }
    // yz plane의 구
    else if (vertexID < NUM_SEGMENTS + 1 + 2 * (NUM_SEGMENTS + 1))
    {
        SegmentIndex = (vertexID - (NUM_SEGMENTS + 1 + NUM_SEGMENTS + 1));
        float SegmentAngle = SegmentIndex / (float) (NUM_SEGMENTS) * (2 * Angle);
        float angleOffset = -Angle;
        localPos3 = float3(cos(angleOffset + SegmentAngle), sin(angleOffset + SegmentAngle), 0);
        localPos3 = localPos3 * float3(radius, radius, radius) * 1;
    }
    // 원점
    else
    {
        localPos3 = float3(0, 0, 0);
    }

    float3 pos = DataCone[instanceID].ApexPosiiton;
    float3x3 rot = CreateRotationMatrixFromX(DataCone[instanceID].Direction);
    
    localPos3 = mul(localPos3, rot);
    localPos3 = localPos3 + pos;
    
    float4 localPos = float4(localPos3, 1.f);
        
    localPos = mul(localPos, ViewMatrix);
    localPos = mul(localPos, ProjectionMatrix);
    output.position = localPos;
    output.color = DataCone[instanceID].Color;
    return output;
}

float4 conePS(PS_INPUT input) : SV_Target
{
    return input.color;
}
/////////////////////////////////////////////
// Grid

struct VS_INPUT_GRID
{
    uint vertexID : SV_VertexID; // 0 또는 1: 각 라인의 시작과 끝
    uint instanceID : SV_InstanceID; // 인스턴스 ID로 grid, axis, bounding box를 구분
};

struct PS_INPUT_GRID
{
    float4 Position : SV_Position;
    float4 WorldPosition : POSITION;
    float4 Color : COLOR;
};

/////////////////////////////////////////////////////////////////////////
// Grid 위치 계산 함수
/////////////////////////////////////////////////////////////////////////
float3 ComputeGridPosition(uint instanceID, uint vertexID)
{
    int halfCount = GridCount / 2;
    float centerOffset = halfCount * 0.5; // grid 중심이 원점에 오도록
    
    float3 startPos;
    float3 endPos;
    
    if (instanceID < halfCount)
    {
        // 수직선: X 좌표 변화, Y는 -centerOffset ~ +centerOffset
        float x = GridOrigin.x + (instanceID - centerOffset) * GridSpacing;
        if (abs(x) < 0.001) // axis와 겹치는 선
        {
            startPos = float3(0, 0, 0);
            endPos = float3(0, (GridOrigin.y - centerOffset * GridSpacing), 0);
        }
        else
        {
            startPos = float3(x, GridOrigin.y - centerOffset * GridSpacing, GridOrigin.z);
            endPos = float3(x, GridOrigin.y + centerOffset * GridSpacing, GridOrigin.z);
        }
    }
    else
    {
        // 수평선: Y 좌표 변화, X는 -centerOffset ~ +centerOffset
        int idx = instanceID - halfCount;
        float y = GridOrigin.y + (idx - centerOffset) * GridSpacing;
        if (abs(y) < 0.001)
        {
            startPos = float3(0, 0, 0);
            endPos = float3(-(GridOrigin.x + centerOffset * GridSpacing), 0, 0);
        }
        else
        {
            startPos = float3(GridOrigin.x - centerOffset * GridSpacing, y, GridOrigin.z);
            endPos = float3(GridOrigin.x + centerOffset * GridSpacing, y, GridOrigin.z);
        }

    }
    return (vertexID == 0) ? startPos : endPos;
}

PS_INPUT_GRID gridVS(VS_INPUT_GRID input)
{
    PS_INPUT_GRID output;
    float3 pos = ComputeGridPosition(input.instanceID, input.vertexID);
    
    output.WorldPosition = float4(pos, 1.f);
    output.Position = mul(output.WorldPosition, ViewMatrix);
    output.Position = mul(output.Position, ProjectionMatrix);
    output.Color = float4(GridColor.xxx, GridAlpha);
    
    return output;
}

float4 gridPS(PS_INPUT_GRID input) : SV_Target
{
    float Dist = length(input.WorldPosition.xyz - ViewWorldLocation);

    float MaxDist = 400 * 1.2f;
    float MinDist = MaxDist * 0.3f;

    // Fade out grid
    float Fade = saturate(1.f - (Dist - MinDist) / (MaxDist - MinDist));
    input.Color.a *= Fade;
    input.Color.rgb *= 2;
    return input.Color;
}


/////////////////////////////////////////////
// Icon
struct PS_INPUT_ICON
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
    float4 Color : COLOR;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

const static float2 QuadPos[6] =
{
    float2(-1, -1), float2(1, -1), float2(-1, 1), // 좌하단, 좌상단, 우하단
    float2(-1, 1), float2(1, -1), float2(1, 1) // 좌상단, 우상단, 우하단
};

const static float2 QuadTexCoord[6] =
{
    float2(0, 1), float2(1, 1), float2(0, 0), // 삼각형 1: 좌하단, 우하단, 좌상단
    float2(0, 0), float2(1, 1), float2(1, 0) // 삼각형 2: 좌상단, 우하단, 우상단
};


PS_INPUT_ICON iconVS(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    PS_INPUT_ICON output;

    float3 IconPosition = IconDatas[instanceID].IconPosition;
    float IconScale = IconDatas[instanceID].IconScale;
    // 카메라를 향하는 billboard 좌표계 생성
    float3 forward = normalize(ViewWorldLocation - IconPosition);
    float3 up = float3(0, 0, 1);
    float3 right = normalize(cross(up, forward));
    up = cross(forward, right);

        // 쿼드 정점 계산 (아이콘 위치 기준으로 offset)
    float2 offset = QuadPos[vertexID];
    float3 worldPos = IconPosition + offset.x * right * IconScale + offset.y * up * IconScale;

        // 변환
    float4 viewPos = mul(float4(worldPos, 1.0), ViewMatrix);
    output.Position = mul(viewPos, ProjectionMatrix);

    output.TexCoord = QuadTexCoord[vertexID];
    output.Color = IconDatas[instanceID].IconColor;
    return output;
}


// 픽셀 셰이더
float4 iconPS(PS_INPUT_ICON input) : SV_Target
{
    float4 iconTexture = gTexture.Sample(gSampler, input.TexCoord);
    float4 color = input.Color / 2;
    color.w = 1.f;
    
    float4 OutColor = iconTexture * color;
    float threshold = 0.01; // 필요한 경우 임계값을 조정
    if (OutColor.a < threshold)
        clip(-1); // 픽셀 버리기
    
    return OutColor;
}


/////////////////////////////////////////////
// Arrow
PS_INPUT arrowVS(VS_INPUT input)
{
    PS_INPUT output;

    // 정규화된 방향
    float3 forward = normalize(ArrowDirection);

    // 기본 up 벡터와 forward가 나란할 때를 방지
    float3 up = abs(forward.y) > 0.99 ? float3(0, 0, 1) : float3(0, 1, 0);

    // 오른쪽 축
    float3 right = normalize(cross(up, forward));

    // 재정의된 up 벡터 (직교화)
    up = normalize(cross(forward, right));

    // 회전 행렬 구성 (Row-Major 기준)
    float3x3 rotationMatrix = float3x3(right, up, forward);

    input.position = input.position * ArrowScaleXYZ;
    input.position.z = input.position.z * ArrowScaleZ;
    // 로컬 → 회전 → 위치
    float3 worldPos = mul(input.position.xyz, rotationMatrix) + ArrowPosition;

    float4 pos = float4(worldPos, 1.0);
    pos = mul(pos, ViewMatrix);
    pos = mul(pos, ProjectionMatrix);

    output.position = pos;
    output.color = ArrowColor;

    return output;
}

float4 arrowPS(PS_INPUT input) : SV_Target
{
    return input.color;
}

// --------- Input/Output 구조체 ----------
struct VS_INPUT_CAPSULE
{
    uint CapsuleIndex : SV_InstanceID;
    uint VertexID : SV_VertexID;
};

// 캡슐 메쉬 분할 수 조절 (높을수록 부드럽게)
#define CAPSULE_RINGS 12
#define CAPSULE_SEGMENTS 24

// 캡슐을 구*2 + 원통으로 분할하여 인스턴스 드로우
PS_INPUT capsuleVS(VS_INPUT_CAPSULE input)
{
    PS_INPUT output;

    // 캡슐 정보 로드
    CapsuleData capsule = DataCapsule[input.CapsuleIndex];
    float3 pa = capsule.PointA;
    float3 pb = capsule.PointB;
    float r = capsule.Radius;

    // 캡슐 축 방향 및 길이
    float3 axis = normalize(pb - pa);
    float len = length(pb - pa);

    // 축을 기준으로 회전행렬 생성 (z축→axis)
    float3 up = float3(0, 0, 1);
    float3 v = cross(up, axis);
    float s = length(v);
    float c = dot(up, axis);
    float3x3 rot = (s < 1e-5) ? float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1) :
        (float3x3(
            v.x * v.x * (1 - c) + c, v.x * v.y * (1 - c) - v.z * s, v.x * v.z * (1 - c) + v.y * s,
            v.y * v.x * (1 - c) + v.z * s, v.y * v.y * (1 - c) + c, v.y * v.z * (1 - c) - v.x * s,
            v.z * v.x * (1 - c) - v.y * s, v.z * v.y * (1 - c) + v.x * s, v.z * v.z * (1 - c) + c
        ));

    // 전체 캡슐 메쉬를 [위구, 실린더(원통), 아래구]로 분할
    uint vertsPerHemisphere = (CAPSULE_RINGS / 2 + 1) * (CAPSULE_SEGMENTS + 1);
    uint vertsPerCylinder = (CAPSULE_RINGS / 2 - 1) * (CAPSULE_SEGMENTS + 1);

    // 인덱스에 따라 위치 계산
    float3 localPos = float3(0, 0, 0);

    // 위쪽 구 (z=+len/2)
    if (input.VertexID < vertsPerHemisphere)
    {
        uint ring = input.VertexID / (CAPSULE_SEGMENTS + 1);
        uint seg = input.VertexID % (CAPSULE_SEGMENTS + 1);
        float v = ring / (float) (CAPSULE_RINGS / 2);
        float theta = v * (3.14159265f / 2); // 0~pi/2
        float u = seg / (float) (CAPSULE_SEGMENTS);
        float phi = u * (2 * 3.14159265f);
        localPos = float3(
            r * sin(theta) * cos(phi),
            r * sin(theta) * sin(phi),
            r * cos(theta) + len / 2
        );
    }
    // 원통
    else if (input.VertexID < vertsPerHemisphere + vertsPerCylinder)
    {
        uint cid = input.VertexID - vertsPerHemisphere;
        uint ring = cid / (CAPSULE_SEGMENTS + 1);
        uint seg = cid % (CAPSULE_SEGMENTS + 1);
        float v = ring / (float) (CAPSULE_RINGS / 2 - 1);
        float z = len / 2 - v * len;
        float u = seg / (float) (CAPSULE_SEGMENTS);
        float phi = u * (2 * 3.14159265f);
        localPos = float3(
            r * cos(phi),
            r * sin(phi),
            z
        );
    }
    // 아래쪽 구 (z=-len/2)
    else
    {
        uint sid = input.VertexID - (vertsPerHemisphere + vertsPerCylinder);
        uint ring = sid / (CAPSULE_SEGMENTS + 1);
        uint seg = sid % (CAPSULE_SEGMENTS + 1);
        float v = ring / (float) (CAPSULE_RINGS / 2);
        float theta = v * (3.14159265f / 2); // 0~pi/2
        float u = seg / (float) (CAPSULE_SEGMENTS);
        float phi = u * (2 * 3.14159265f);
        localPos = float3(
            r * sin(theta) * cos(phi),
            r * sin(theta) * sin(phi),
            -r * cos(theta) - len / 2
        );
    }

    // 축방향 회전 및 위치 이동
    float3 worldPos = mul(rot, localPos) + (pa + pb) * 0.5;

    // 뷰/프로젝션 매트릭스 적용
    float4 viewPos = mul(ViewMatrix, float4(worldPos, 1));
    output.position = mul(ProjectionMatrix, viewPos);
    output.color = float4(0, 1, 0, 1);
    return output;
}

float4 capsulePS(PS_INPUT input) : SV_Target
{
    return input.color;
}

const static float3 PyramidPos[5] =
{
    float3(1, 1, 0),
    float3(-1, 1, 0),
    float3(-1, -1, 0),
    float3(1, -1, 0),
    float3(0, 0, 1)
};

// 각 모서리를 이루는 점 두 개의 인덱스 (16개, 8개의 라인)
static const uint PyramidEdgeIndices[16] =
{
    // 밑면 사각형
    0, 1,
    1, 2,
    2, 3,
    3, 0,
    // 옆면
    0, 4,
    1, 4,
    2, 4,
    3, 4
};

float3 GetUpVector(float3 dir)
{
    // direction이 0,0,0일 경우 기본 up
    if (all(dir == float3(0, 0, 0)))
        return float3(0, 0, 1);
    return normalize(dir);
}

// direction 기준 회전 행렬 생성 (z축 -> direction)
float3x3 GetRotationMatrix(float3 direction)
{
    float3 up = GetUpVector(direction);

    // 보조 축 계산 (z축과 direction 평행시 문제 방지)
    float3 temp = abs(up.z) < 0.99 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 right = normalize(cross(temp, up));
    float3 forward = cross(up, right);

    // 열벡터 방식: (right, forward, up)
    return float3x3(right, forward, up);
}

// Quad Pyramid
PS_INPUT pyramidVS(uint VertexID : SV_VertexID, uint InstanceID : SV_InstanceID)
{
    PS_INPUT output;

    uint localVertexIdx = VertexID % 16;
    uint pyramidIdx = InstanceID;
    uint posIdx = PyramidEdgeIndices[localVertexIdx];
    float3 localPos = PyramidPos[posIdx];

    PyramidData pd = DataPyramid[pyramidIdx];

    float3 scale = float3(pd.SquareSize.xx, pd.Height);
    float3 modelPos = localPos * scale;

    // direction에 따라 회전
    float3x3 rot = GetRotationMatrix(pd.Direction);
    float3 worldOffset = mul(modelPos, rot);

    float3 worldPos = pd.Position + worldOffset;

    // View/Proj 변환은 별도 상수버퍼에서 처리
    output.position = float4(worldPos, 1);
    output.position = mul(output.position, ViewMatrix);
    output.position = mul(output.position, ProjectionMatrix);
    
    output.color = pd.Color;

    return output;
}

float4 pyramidPS(PS_INPUT input) : SV_Target
{
    return input.color;
}
