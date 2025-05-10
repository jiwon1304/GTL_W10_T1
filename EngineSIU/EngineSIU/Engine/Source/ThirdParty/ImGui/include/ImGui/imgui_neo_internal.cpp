//
// Created by Matty on 2022-01-28.
//

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui_neo_internal.h"
#include "imgui_internal.h"
#include <cstdint>

namespace ImGui {
    void RenderNeoSequencerBackground(const ImVec4 &color, const ImVec2 & cursor, const ImVec2 &size, ImDrawList * drawList, float sequencerRounding) {
        if(!drawList)
        {
            drawList = ImGui::GetWindowDrawList();
        }

        const ImRect area = {cursor, cursor + size};

        drawList->AddRectFilled(area.Min, area.Max, ColorConvertFloat4ToU32(color), sequencerRounding);
    }

    void RenderNeoSequencerTopBarBackground(const ImVec4 &color, const ImVec2 &cursor, const ImVec2 &size,
                                            ImDrawList *drawList, float sequencerRounding) {
        if(!drawList)
        {
            drawList = ImGui::GetWindowDrawList();
        }

        const ImRect barArea = {cursor, cursor + size};

        drawList->AddRectFilled(barArea.Min, barArea.Max, ColorConvertFloat4ToU32(color), sequencerRounding);
    }

    void
    RenderNeoSequencerTopBarOverlay(float zoom, float valuesWidth,float_t startFrame, float_t endFrame, float_t offsetFrame, const ImVec2 &cursor, const ImVec2 &size,
                                    ImDrawList *drawList, bool drawFrameLines,
                                    bool drawFrameText, float maxPixelsPerTick) {
        if(!drawList)
        {
            drawList = ImGui::GetWindowDrawList();
        }

        const auto & style = GetStyle();

        const ImRect barArea = {cursor + ImVec2{style.FramePadding.x + valuesWidth,style.FramePadding.y}, cursor + size };

        const float_t viewEnd = endFrame + offsetFrame;
        const float_t viewStart = startFrame + offsetFrame;

        if(drawFrameLines) {
            const auto count = static_cast<int32_t>(((viewEnd + 1) - viewStart) / zoom);

            float_t counter = 0;
            float_t primaryFrames = powf(10.0f, counter++);
            float_t secondaryFrames = powf(10.0f, counter);

            float perFrameWidth = GetPerFrameWidth(size.x, valuesWidth, endFrame, startFrame, zoom);

            if(perFrameWidth <= 0.0f)
            {
                return;
            }

            while (perFrameWidth < maxPixelsPerTick)
            {
                primaryFrames = powf(10.0f, counter++);
                secondaryFrames = powf(10.0f, counter);

                perFrameWidth *= (float)primaryFrames;
            }

            if(primaryFrames == 0.0f || secondaryFrames == 0.0f) {
                primaryFrames = 1;
                secondaryFrames = 10;
            }

            for(int32_t i = 0; i < count; i++) {
                // 현재 프레임이 주 눈금(secondaryFrames)이나 보조 눈금(primaryFrames)에 해당하는지 확인
                const bool isPrimaryTick = (fmod(viewStart + static_cast<float_t>(i), primaryFrames) < 0.001f);
                const bool isSecondaryTick = (fmod(viewStart + static_cast<float_t>(i), secondaryFrames) < 0.001f);

                // 어느 눈금에도 해당하지 않으면 건너뜀
                if(!isPrimaryTick && !isSecondaryTick)
                {
                    continue;
                }

                // 눈금 높이 결정 (주 눈금은 전체 높이, 보조 눈금은 절반 높이)
                const float barHeight = barArea.GetSize().y;
                const float lineHeight = isSecondaryTick ? barHeight : barHeight / 2.0f;
    
                // 눈금 위치 계산
                const float xPos = barArea.Min.x + static_cast<float>(i) * (perFrameWidth / (float)primaryFrames);
                const ImVec2 p1 = {xPos, barArea.Max.y};
                const ImVec2 p2 = {xPos, barArea.Max.y - lineHeight};

                // 선 그리기
                drawList->AddLine(p1, p2, IM_COL32_WHITE, 1.0f);

                // 주 눈금에만 텍스트 표시
                if(drawFrameText && isSecondaryTick) {
                    char text[16];
                    int frameNum = static_cast<int>(viewStart) + i;
                    if(snprintf(text, sizeof(text), "%i", frameNum) > 0) {
                        drawList->AddText(nullptr, 0, ImVec2(xPos + 2.0f, barArea.Min.y), IM_COL32_WHITE, text);
                    }
                }
            }
        }
    }

    void RenderNeoTimelineLabel(const char * label,const ImVec2 & cursor,const ImVec2 & size, const ImVec4& color,bool isGroup, bool isOpen, ImDrawList *drawList)
    {
        const auto& imStyle = GetStyle();

        if(!drawList)
        {
            drawList = ImGui::GetWindowDrawList();
        }

        auto c = cursor;

        if(isGroup) {
            RenderArrow(drawList,c,IM_COL32_WHITE,isOpen ? ImGuiDir_Down : ImGuiDir_Right);
            c.x += size.y + imStyle.ItemSpacing.x;
        }

        drawList->AddText(c,ColorConvertFloat4ToU32(color),label, FindRenderedTextEnd(label));
    }

    void RenderNeoTimelinesBorder(const ImVec4 &color, const ImVec2 &cursor, const ImVec2 &size, ImDrawList *drawList,
                                  float rounding, float borderSize)
    {
        if(!drawList)
        {
            drawList = ImGui::GetWindowDrawList();
        }

        drawList->AddRect(cursor,cursor + size,ColorConvertFloat4ToU32(color),rounding, 0, borderSize);
    }

    void RenderNeoTimeline(bool selected,const ImVec2 & cursor, const ImVec2& size, const ImVec4& highlightColor, ImDrawList *drawList) {
        if(!drawList)
        {
            drawList = ImGui::GetWindowDrawList();
        }

        if(selected) {
            const ImRect area = {cursor, cursor + size};
            drawList->AddRectFilled(area.Min, area.Max, ColorConvertFloat4ToU32(highlightColor));
        }
    }

    float GetPerFrameWidth(float totalSizeX, float valuesWidth, float_t endFrame, float_t startFrame, float zoom) {
        const auto& imStyle = GetStyle();

        const auto size = totalSizeX - valuesWidth - imStyle.FramePadding.x;

        auto count = (endFrame + 1) - startFrame;

        return ((size / (float)count) * zoom);
    }

    struct Vec2Pair {
        ImVec2 a;
        ImVec2 b;
    };

    static Vec2Pair getCurrentFrameLine(const ImRect & pointerBB, float timelineHeight) {
        const auto center = ImVec2{pointerBB.Min.x, pointerBB.Max.y} + ImVec2{pointerBB.GetSize().x / 2.0f, 0};

        return Vec2Pair{ center, center + ImVec2{0, timelineHeight} };
    }

    void RenderNeoSequencerCurrentFrame(const ImVec4 &color, const ImVec4 &topColor, const ImRect &pointerBB,
                                               float timelineHeight, float lineWidth, ImDrawList *drawList) {
        if(!drawList)
        {
            drawList = ImGui::GetWindowDrawList();
        }

        const auto pair = getCurrentFrameLine(pointerBB, timelineHeight);

        drawList->AddLine(pair.a, pair.b, ColorConvertFloat4ToU32(color), lineWidth);

        drawList->PopClipRect();

        { //Top pointer has custom shape, we have to create it
            const auto size = pointerBB.GetSize();
            ImVec2 pts[5];
            pts[0] = pointerBB.Min;
            pts[1] = pointerBB.Min + ImVec2{size.x, 0};
            pts[2] = pointerBB.Min + ImVec2{size.x, size.y * 0.85f};
            pts[3] = pointerBB.Min + ImVec2{size.x / 2, size.y};
            pts[4] = pointerBB.Min + ImVec2{0, size.y * 0.85f};

            drawList->AddConvexPolyFilled(pts, sizeof(pts) / sizeof(*pts), ColorConvertFloat4ToU32(topColor));
        }
    }
}
