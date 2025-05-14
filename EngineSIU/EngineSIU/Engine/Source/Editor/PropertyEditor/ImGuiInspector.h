#pragma once
class UField;
class UObject;

namespace ImGuiInspector
{
    /// 한 필드를 화면에 그려주는 함수.
    void DrawFieldEditor(UField* Field, UObject*& ObjPtr);
}
