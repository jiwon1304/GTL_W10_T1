#include "DXDBufferManager.h"

#include <codecvt>
#include <locale>

void FDXDBufferManager::Initialize(ID3D11Device* InDXDevice, ID3D11DeviceContext* InDXDeviceContext)
{
    DXDevice = InDXDevice;
    DXDeviceContext = InDXDeviceContext;
    CreateQuadBuffer();
}

void FDXDBufferManager::ReleaseBuffers()
{
    for (auto& Pair : VertexBufferPool)
    {
        if (Pair.Value.VertexBuffer)
        {
            Pair.Value.VertexBuffer->Release();
            Pair.Value.VertexBuffer = nullptr;
        }
    }
    VertexBufferPool.Empty();

    for (auto& Pair : IndexBufferPool)
    {
        if (Pair.Value.IndexBuffer)
        {
            Pair.Value.IndexBuffer->Release();
            Pair.Value.IndexBuffer = nullptr;
        }
    }
    IndexBufferPool.Empty();
}

void FDXDBufferManager::ReleaseConstantBuffer()
{
    for (auto& Pair : ConstantBufferPool)
    {
        if (Pair.Value)
        {
            Pair.Value->Release();
            Pair.Value = nullptr;
        }
    }
    ConstantBufferPool.Empty();
}

void FDXDBufferManager::BindConstantBuffers(const TArray<FString>& Keys, UINT StartSlot, EShaderStage Stage) const
{
    const int Count = Keys.Num();
    TArray<ID3D11Buffer*> Buffers;
    Buffers.Reserve(Count);
    for (const FString& Key : Keys)
    {
        ID3D11Buffer* Buffer = GetConstantBuffer(Key);
        Buffers.Add(Buffer);
    }

    if (Stage == EShaderStage::Vertex)
    {
        DXDeviceContext->VSSetConstantBuffers(StartSlot, Count, Buffers.GetData());
    }
    else if (Stage == EShaderStage::Pixel)
    {
        DXDeviceContext->PSSetConstantBuffers(StartSlot, Count, Buffers.GetData());
    }
    else if (Stage == EShaderStage::Compute)
    {
        DXDeviceContext->CSSetConstantBuffers(StartSlot, Count, Buffers.GetData());
    }
    else if (Stage == EShaderStage::Geometry)
    {
        DXDeviceContext->GSSetConstantBuffers(StartSlot, Count, Buffers.GetData());
    }
}   

void FDXDBufferManager::BindConstantBuffer(const FString& Key, UINT StartSlot, EShaderStage Stage) const
{
    ID3D11Buffer* Buffer = GetConstantBuffer(Key);
    if (Stage == EShaderStage::Vertex)
        DXDeviceContext->VSSetConstantBuffers(StartSlot, 1, &Buffer);
    else if (Stage == EShaderStage::Pixel)
        DXDeviceContext->PSSetConstantBuffers(StartSlot, 1, &Buffer);
    else if (Stage == EShaderStage::Compute)
        DXDeviceContext->CSSetConstantBuffers(StartSlot, 1, &Buffer);
    else if (Stage == EShaderStage::Geometry)
        DXDeviceContext->GSSetConstantBuffers(StartSlot, 1, &Buffer);
}

void FDXDBufferManager::SetVertexBuffer(const FString& InName, ID3D11DeviceContext* DeviceContext)
{
    ID3D11Buffer* Buffer = nullptr;
    UINT Stride;
    UINT Offset;

    if (!VertexBufferPool.Contains(InName))
    {
        UE_LOG(ELogLevel::Error, "Failed to set vertex shader : invalid key");
        return;
    }

    FVertexInfo& VertexInfo = VertexBufferPool[InName];
    Stride = VertexInfo.Stride;
    Offset = 0;
    Buffer = VertexInfo.VertexBuffer;

    if (DeviceContext)
    {
        DeviceContext->IASetVertexBuffers(0, 1, &Buffer, &Stride, &Offset);
    }
    else
    {
        if (!DXDeviceContext)
        {
            UE_LOG(ELogLevel::Error, "Failed to set vertex shader : Default DeviceContext not exist");
        }

        DXDeviceContext->IASetVertexBuffers(0, 1, &Buffer, &Stride, &Offset);
    }
}

void FDXDBufferManager::SetIndexBuffer(const FString& InName, ID3D11DeviceContext* DeviceContext)
{
    ID3D11Buffer* Buffer = nullptr;

    if (!IndexBufferPool.Contains(InName))
    {
        UE_LOG(ELogLevel::Error, "Failed to set index shader : invalid key");
        return;
    }

    FIndexInfo& VertexInfo = IndexBufferPool[InName];
    Buffer = VertexInfo.IndexBuffer;

    if (DeviceContext)
    {
        DeviceContext->IASetIndexBuffer(Buffer, DXGI_FORMAT_R32_UINT, 0);
    }
    else
    {
        if (!DXDeviceContext)
        {
            UE_LOG(ELogLevel::Error, "Failed to set vertex shader : Default DeviceContext not exist");
        }

        DXDeviceContext->IASetIndexBuffer(Buffer, DXGI_FORMAT_R32_UINT, 0);
    }
}

FVertexInfo FDXDBufferManager::GetVertexBuffer(const FString& InName) const
{
    if (VertexBufferPool.Contains(InName))
        return VertexBufferPool[InName];
    return FVertexInfo();
}

FIndexInfo FDXDBufferManager::GetIndexBuffer(const FString& InName) const
{
    if (IndexBufferPool.Contains(InName))
        return IndexBufferPool[InName];
    return FIndexInfo();
}

FVertexInfo FDXDBufferManager::GetTextVertexBuffer(const FWString& InName) const
{
    if (TextAtlasVertexBufferPool.Contains(InName))
        return TextAtlasVertexBufferPool[InName];

    return FVertexInfo();
}

FIndexInfo FDXDBufferManager::GetTextIndexBuffer(const FWString& InName) const
{
    if (TextAtlasIndexBufferPool.Contains(InName))
        return TextAtlasIndexBufferPool[InName];

    return FIndexInfo();
}


ID3D11Buffer* FDXDBufferManager::GetConstantBuffer(const FString& InName) const
{
    if (ConstantBufferPool.Contains(InName))
        return ConstantBufferPool[InName];

    return nullptr;
}

void FDXDBufferManager::CreateQuadBuffer()
{
    TArray<QuadVertex> Vertices =
    {
        { {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },
        { { 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
        { { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
        { {-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f} },
    };

    FVertexInfo VertexInfo;
    CreateVertexBuffer(TEXT("QuadBuffer"), Vertices, VertexInfo);

    TArray<short> Indices =
    {
        0, 1, 2,
        0, 2, 3
    };

    FIndexInfo IndexInfo;
    CreateIndexBuffer(TEXT("QuadBuffer"), Indices, IndexInfo);
}

void FDXDBufferManager::GetQuadBuffer(FVertexInfo& OutVertexInfo, FIndexInfo& OutIndexInfo)
{
    OutVertexInfo = GetVertexBuffer(TEXT("QuadBuffer"));
    OutIndexInfo = GetIndexBuffer(TEXT("QuadBuffer"));
}

void FDXDBufferManager::GetTextBuffer(const FWString& Text, FVertexInfo& OutVertexInfo, FIndexInfo& OutIndexInfo)
{
    OutVertexInfo = GetTextVertexBuffer(Text);
    OutIndexInfo = GetTextIndexBuffer(Text);
}


HRESULT FDXDBufferManager::CreateUnicodeTextBuffer(const FWString& Text, FBufferInfo& OutBufferInfo,
    float BitmapWidth, float BitmapHeight, float ColCount, float RowCount)
{
    if (TextAtlasBufferPool.Contains(Text))
    {
        OutBufferInfo = TextAtlasBufferPool[Text];
        return S_OK;
    }

    TArray<FVertexTexture> Vertices;

    // 각 글자에 대한 기본 쿼드 크기 (폭과 높이)
    const float quadWidth = 2.0f;
    const float quadHeight = 2.0f;

    // 전체 텍스트의 너비
    float totalTextWidth = quadWidth * Text.size();
    // 텍스트의 중앙으로 정렬하기 위한 오프셋
    float centerOffset = totalTextWidth / 2.0f;

    float CellWidth = float(BitmapWidth) / ColCount; // 컬럼별 셀 폭
    float CellHeight = float(BitmapHeight) / RowCount; // 행별 셀 높이

    float nTexelUOffset = CellWidth / BitmapWidth;
    float nTexelVOffset = CellHeight / BitmapHeight;

    for (int i = 0; i < Text.size(); i++)
    {
        // 각 글자에 대해 기본적인 사각형 좌표 설정 (원점은 -1.0f부터 시작)
        FVertexTexture leftUP = { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f };
        FVertexTexture rightUP = { 1.0f,  1.0f, 0.0f, 1.0f, 0.0f };
        FVertexTexture leftDown = { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f };
        FVertexTexture rightDown = { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f };

        // UV 좌표 관련 보정 (nTexel 오프셋 적용)
        rightUP.u *= nTexelUOffset;
        leftDown.v *= nTexelVOffset;
        rightDown.u *= nTexelUOffset;
        rightDown.v *= nTexelVOffset;

        // 각 글자의 x 좌표에 대해 오프셋 적용 (중앙 정렬을 위해 centerOffset 만큼 빼줌)
        float xOffset = quadWidth * i - centerOffset;
        leftUP.x += xOffset;
        rightUP.x += xOffset;
        leftDown.x += xOffset;
        rightDown.x += xOffset;

        FVector2D UVOffset;
        SetStartUV(Text[i], UVOffset);

        leftUP.u += (nTexelUOffset * UVOffset.X);
        leftUP.v += (nTexelVOffset * UVOffset.Y);
        rightUP.u += (nTexelUOffset * UVOffset.X);
        rightUP.v += (nTexelVOffset * UVOffset.Y);
        leftDown.u += (nTexelUOffset * UVOffset.X);
        leftDown.v += (nTexelVOffset * UVOffset.Y);
        rightDown.u += (nTexelUOffset * UVOffset.X);
        rightDown.v += (nTexelVOffset * UVOffset.Y);

        // 각 글자의 쿼드를 두 개의 삼각형으로 생성
        Vertices.Add(leftUP);
        Vertices.Add(rightUP);
        Vertices.Add(leftDown);
        Vertices.Add(rightUP);
        Vertices.Add(rightDown);
        Vertices.Add(leftDown);
    }

    CreateVertexBuffer(Text, Vertices, OutBufferInfo.VertexInfo);
    TextAtlasBufferPool[Text] = OutBufferInfo;

    return S_OK;
}

void FDXDBufferManager::SetStartUV(wchar_t hangul, FVector2D& UVOffset)
{
    //대문자만 받는중
    int StartU = 0;
    int StartV = 0;
    int offset = -1;

    if (hangul == L' ') {
        UVOffset = FVector2D(0, 0);  // Space는 특별히 UV 좌표를 (0,0)으로 설정
        offset = 0;
        return;
    }
    else if (hangul >= L'A' && hangul <= L'Z') {

        StartU = 11;
        StartV = 0;
        offset = hangul - L'A'; // 대문자 위치
    }
    else if (hangul >= L'a' && hangul <= L'z') {
        StartU = 37;
        StartV = 0;
        offset = (hangul - L'a'); // 소문자는 대문자 다음 위치
    }
    else if (hangul >= L'0' && hangul <= L'9') {
        StartU = 1;
        StartV = 0;
        offset = (hangul - L'0'); // 숫자는 소문자 다음 위치
    }
    else if (hangul >= L'가' && hangul <= L'힣')
    {
        StartU = 63;
        StartV = 0;
        offset = hangul - L'가'; // 대문자 위치
    }

    if (offset == -1)
    {
        UE_LOG(ELogLevel::Warning, "Text Error");
    }

    int offsetV = (offset + StartU) / 106;
    int offsetU = (offset + StartU) % 106;

    UVOffset = FVector2D(offsetU, StartV + offsetV);

}
