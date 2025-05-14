#include "ImGuiInspector.h"

#include "Components/SceneComponent.h"
#include "Components/Light/PointLightComponent.h"
#include "UObject/Class.h"
#include "ImGui/ImGui.h"
#include "UnrealEd/ImGuiWidget.h"
#include "UObject/Casts.h"
#include "PropertyEditorPanel.h"

namespace ImGuiInspector
{
    void DrawFieldEditor(UField* Field, UObject*& ObjPtr)
    {
        if ((Field->Flags & EditAnywhere) == 0)
            return;

        auto GetSceneComp = [&]() -> USceneComponent*
            {
                return Cast<USceneComponent>(ObjPtr);
            };

        switch (Field->PropType)
        {
        case EPropertyType::Int32:
        {
            auto* FI = static_cast<TField<int32>*>(Field);
            int32  Val = FI->GetValue(ObjPtr);
            if (ImGui::DragInt(*Field->Name, &Val, 1.0f))
                FI->SetValue(ObjPtr, Val);
            break;
        }
        case EPropertyType::Float:
        {
            auto* FF = static_cast<TField<float>*>(Field);
            float  Val = FF->GetValue(ObjPtr);
            if (ImGui::DragFloat(*Field->Name, &Val, 0.1f))
                FF->SetValue(ObjPtr, Val);
            break;
        }
        case EPropertyType::Bool:
        {
            auto* FB = static_cast<TField<bool>*>(Field);
            bool   Val = FB->GetValue(ObjPtr);
            if (ImGui::Checkbox(*Field->Name, &Val))
                FB->SetValue(ObjPtr, Val);
            break;
        }
        case EPropertyType::Struct:
        {
            break;
        }
        case EPropertyType::Vector:
        {
            TField<FVector>* FV = dynamic_cast<TField<FVector>*>(Field);
            FVector Vec = FV->GetValue(ObjPtr);
            FImGuiWidget::DrawVec3Control(*Field->Name, Vec, 0, 85);
            FV->SetValue(ObjPtr, Vec);
            if (auto* SC = GetSceneComp())
            {
                if (Field->Name == TEXT("RelativeLocation"))
                {
                    SC->SetRelativeLocation(Vec);
                }
                else if (Field->Name == TEXT("RelativeScale3D"))
                {
                    SC->SetRelativeScale3D(Vec);
                }
            }
            break;
        }
        case EPropertyType::Rotator:
        {
            TField<FRotator>* FR = dynamic_cast<TField<FRotator>*>(Field);
            FRotator R = FR->GetValue(ObjPtr);
            FImGuiWidget::DrawRot3Control(*Field->Name, R, 0, 85);
            FR->SetValue(ObjPtr, R);
            if (auto* SC = GetSceneComp())
            {
                SC->SetRelativeRotationUnsafe(R);
            }
            break;
        }
        case EPropertyType::Color:
        {
            TField<FLinearColor>* FC = dynamic_cast<TField<FLinearColor>*>(Field);
            FLinearColor Color = FC->GetValue(ObjPtr);
            float R = Color.R, G = Color.G, B = Color.B, A = Color.A;
            float LightColor[4] = { R, G, B, A };

            if (ImGui::ColorPicker4(*Field->Name, LightColor,
                ImGuiColorEditFlags_DisplayRGB |
                ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_Float))
            {
                Color = FLinearColor(LightColor[0], LightColor[1], LightColor[2], LightColor[3]);
                FC->SetValue(ObjPtr, Color);
            }

            float H, S, V;
            PropertyEditorPanel::RGBToHSV(R, G, B, H, S, V);

            bool ChangedRGB = false;
            bool ChangedHSV = false;

            ImGui::PushItemWidth(50.0f);
            if (ImGui::DragFloat("R##R", &R, 0.001f, 0.f, 1.f)) ChangedRGB = true;
            ImGui::SameLine();
            if (ImGui::DragFloat("G##G", &G, 0.001f, 0.f, 1.f)) ChangedRGB = true;
            ImGui::SameLine();
            if (ImGui::DragFloat("B##B", &B, 0.001f, 0.f, 1.f)) ChangedRGB = true;
            ImGui::Spacing();

            if (ImGui::DragFloat("H##H", &H, 0.1f, 0.f, 360)) ChangedHSV = true;
            ImGui::SameLine();
            if (ImGui::DragFloat("S##S", &S, 0.001f, 0.f, 1.f)) ChangedHSV = true;
            ImGui::SameLine();
            if (ImGui::DragFloat("V##V", &V, 0.001f, 0.f, 1.f)) ChangedHSV = true;
            ImGui::PopItemWidth();
            ImGui::Spacing();

            if (ChangedRGB)
            {
                FC->SetValue(ObjPtr, FLinearColor(R, G, B, A));
            }
            else if (ChangedHSV)
            {
                PropertyEditorPanel::HSVToRGB(H, S, V, R, G, B);
                FC->SetValue(ObjPtr, FLinearColor(R, G, B, A));
            }

            break;
        }
        default:
            ImGui::Text("%s: (unsupported)", *Field->Name);
            break;
        }
    }
}

