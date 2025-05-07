#include "SSplitter.h"
#include "EngineLoop.h"

extern FEngineLoop GEngineLoop;

void SSplitter::Initialize(FRect InitRect)
{
    __super::Initialize(InitRect);
    
    if (SideLT == nullptr)
    {
        SideLT = new SWindow();
    }
    if (SideRB == nullptr)
    {
        SideRB = new SWindow();
    }
}

void SSplitter::OnResize(uint32 InWidth, uint32 InHeight)
{
    Rect.Width = static_cast<float>(InWidth);
    Rect.Height = static_cast<float>(InHeight);
}

void SSplitter::SetRect(const FRect& InRect)
{
    Rect = InRect;
}

bool SSplitter::OnPressed(const FPoint& InPoint)
{
    if (!IsHover(InPoint))
    {
        return false;
    }

    bIsSplitterPressed = IsSplitterHovered(InPoint);
    
    return bIsPressed = true;
}

bool SSplitter::OnReleased()
{
    bIsPressed = false;
    bIsSplitterPressed = false;
    
    return false;
}

bool SSplitter::IsSplitterHovered(const FPoint& InPoint)
{
    // 기본 구현: 항상 false, 실제 판정은 SSplitterV/H에서 override
    return false;
}

// 수직 스플리터: 수평 방향으로 Splitter 바 영역 판정
bool SSplitterV::IsSplitterHovered(const FPoint& InPoint)
{
    float centerX = GetSplitterLTCenter();
    float minX = centerX - SplitterHalfThickness;
    float maxX = centerX + SplitterHalfThickness;
    return (InPoint.x >= minX && InPoint.x <= maxX);
}

// 수평 스플리터: 수직 방향으로 Splitter 바 영역 판정
bool SSplitterH::IsSplitterHovered(const FPoint& InPoint)
{
    float centerY = GetSplitterLTCenter();
    float minY = centerY - SplitterHalfThickness;
    float maxY = centerY + SplitterHalfThickness;
    return (InPoint.y >= minY && InPoint.y <= maxY);
}

void SSplitter::LoadConfig(const TMap<FString, FString>& Config, FString Key, float DefaultValue) {}
void SSplitter::SaveConfig(TMap<FString, FString>& Config, FString Key) const {}


void SSplitterV::Initialize(FRect InRect)
{
    __super::Initialize(InRect);

    UpdateChildRects();
}

void SSplitterV::ClampSplitRatio()
{
    SplitRatio = FMath::Max(SplitRatio, static_cast<float>(SplitterLimitLT) / Rect.Width);
    SplitRatio = FMath::Min(SplitRatio, (Rect.Width - static_cast<float>(SplitterLimitLT)) / Rect.Width);
}

float SSplitterV::GetSplitterLTCenter()
{
    ClampSplitRatio();
    return Rect.Width * SplitRatio;
}

void SSplitterV::LoadConfig(const TMap<FString, FString>& Config, FString Key, const float DefaultValue)
{
    SplitRatio = GetValueFromConfig(Config, Key, DefaultValue);

    UpdateChildRects();
}

void SSplitterV::SaveConfig(TMap<FString, FString>& Config, FString Key) const
{
    Config[Key] = std::to_string(SplitRatio);
}

void SSplitterV::OnResize(uint32 InWidth, uint32 InHeight)
{
    __super::OnResize(InWidth, InHeight);
    UpdateChildRects();
}

void SSplitterV::SetRect(const FRect& InRect)
{
    Rect = InRect;
    UpdateChildRects();
}

void SSplitterV::OnDrag(const FPoint& Delta)
{
    // 수평 스플리터의 경우, 좌우로 이동
    float CenterX = GetSplitterLTCenter();
    CenterX += Delta.x;

    // 픽셀 단위 이동을 위해 정수형으로 변환 후 계산
    SplitRatio = std::trunc(CenterX) / Rect.Width;
    
    UpdateChildRects();
}

void SSplitterV::UpdateChildRects()
{
    // 픽셀 단위로 계산하기 위해 정수형으로 변환
    const uint32 SplitterCenterX = static_cast<uint32>(GetSplitterLTCenter());
    
    if (SideLT)
    {
        SideLT->Initialize(FRect(
            std::trunc(Rect.TopLeftX),
            std::trunc(Rect.TopLeftY),
            static_cast<float>(SplitterCenterX - SplitterHalfThickness),
            std::trunc(Rect.Height)
        ));
    }
    if (SideRB)
    {
        const float Offset = static_cast<float>(SplitterCenterX + SplitterHalfThickness);
        
        SideRB->Initialize(FRect(
            std::trunc(Rect.TopLeftX) + Offset,
            std::trunc(Rect.TopLeftY),
            std::trunc(Rect.Width - Offset),
            std::trunc(Rect.Height)
        ));
    }
}

void SSplitterH::Initialize(FRect InRect)
{
    __super::Initialize(InRect);

    UpdateChildRects();
}

void SSplitterH::ClampSplitRatio()
{
    SplitRatio = FMath::Max(SplitRatio, static_cast<float>(SplitterLimitLT) / Rect.Height);
    SplitRatio = FMath::Min(SplitRatio, (Rect.Height - static_cast<float>(SplitterLimitLT)) / Rect.Height);
}

float SSplitterH::GetSplitterLTCenter()
{
    ClampSplitRatio();
    return Rect.Height * SplitRatio;
}

void SSplitterH::LoadConfig(const TMap<FString, FString>& Config, FString Key, const float DefaultValue)
{
    SplitRatio = GetValueFromConfig(Config, Key, DefaultValue);

    UpdateChildRects();
}

void SSplitterH::SaveConfig(TMap<FString, FString>& Config, FString Key) const
{
    Config[Key] = std::to_string(SplitRatio);
}

void SSplitterH::OnResize(uint32 InWidth, uint32 InHeight)
{
    __super::OnResize(InWidth, InHeight);
    UpdateChildRects();
}

void SSplitterH::SetRect(const FRect& InRect)
{
    Rect = InRect;
    UpdateChildRects();
}

void SSplitterH::OnDrag(const FPoint& Delta)
{
    float CenterY = GetSplitterLTCenter();
    CenterY += Delta.y;

    // 픽셀 단위 이동을 위해 정수형으로 변환 후 계산
    SplitRatio = std::trunc(CenterY) / Rect.Height;
    
    UpdateChildRects();
}

void SSplitterH::UpdateChildRects()
{
    // 픽셀 단위로 계산하기 위해 정수형으로 변환
    const uint32 SplitterCenterY = static_cast<uint32>(GetSplitterLTCenter());
    
    if (SideLT)
    {
        SideLT->Initialize(FRect(
            std::trunc(Rect.TopLeftX),
            std::trunc(Rect.TopLeftY),
            std::trunc(Rect.Width),
            static_cast<float>(SplitterCenterY - SplitterHalfThickness)
        ));
    }
    if (SideRB)
    {
        const float Offset = static_cast<float>(SplitterCenterY + SplitterHalfThickness);
        
        SideRB->Initialize(FRect(
            std::trunc(Rect.TopLeftX),
            std::trunc(Rect.TopLeftY) + Offset,
            std::trunc(Rect.Width),
            std::trunc(Rect.Height - Offset)
        ));
    }
}
