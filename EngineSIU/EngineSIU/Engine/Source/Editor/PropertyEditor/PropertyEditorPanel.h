#pragma once

#include "Actors/Player.h"
#include "Components/ActorComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "UnrealEd/EditorPanel.h"
#include "Math/Rotator.h"
#include "UObject/Casts.h"
#include "Components/Material/Material.h"

#include "ImGuiInspector.h"

class USpringArmComponent;
class UShapeComponent;
class UAmbientLightComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class USpotLightComponent;
class ULightComponentBase;
class UProjectileMovementComponent;
class UTextComponent;
class UHeightFogComponent;
class AEditorPlayer;
class UStaticMeshComponent;

// 헬퍼 함수 예시
template<typename Getter, typename Setter>
void DrawColorProperty(const char* Label, Getter Get, Setter Set)
{
    ImGui::PushItemWidth(200.0f);
    const FLinearColor CurrentColor = Get();
    float Col[4] = { CurrentColor.R, CurrentColor.G, CurrentColor.B, CurrentColor.A };

    if (ImGui::ColorEdit4(Label, Col,
        ImGuiColorEditFlags_DisplayRGB |
        ImGuiColorEditFlags_NoSidePreview |
        ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_Float))
    {
        Set(FLinearColor(Col[0], Col[1], Col[2], Col[3]));
    }
    ImGui::PopItemWidth();
}


class PropertyEditorPanel : public UEditorPanel
{
public:
    virtual void Render() override;
    virtual void OnResize(HWND hWnd) override;

    static void RGBToHSV(float R, float G, float B, float& H, float& S, float& V);
    static void HSVToRGB(float H, float S, float V, float& R, float& G, float& B);

private:

    void RenderForSceneComponent(USceneComponent* SceneComponent, AEditorPlayer* Player);
    void RenderForCameraComponent(UCameraComponent* InCameraComponent);
    void RenderForPlayerActor(APlayer* InPlayerActor);
    void RenderForActor(AActor* SelectedActor, USceneComponent* TargetComponent) const;
    
    /* Static Mesh Settings */
    void RenderForStaticMesh(UStaticMeshComponent* StaticMeshComp) const;
    void DrawAnimationControls(USkeletalMeshComponent* SkeletalComp);
    void DrawStateMachineDebugControls(USkeletalMeshComponent* SkeletalComp);
    void RenderForSkeletalMesh(USkeletalMeshComponent* SkeletalComp);
    void RenderForModifySkeletalBone(USkeletalMeshComponent* SkeletalMeshComponent);

    void RenderForAmbientLightComponent(UAmbientLightComponent* AmbientLightComponent) const;
    void RenderForDirectionalLightComponent(UDirectionalLightComponent* DirectionalLightComponent) const;
    void RenderForPointLightComponent(UPointLightComponent* PointlightComponent) const;
    void RenderForSpotLightComponent(USpotLightComponent* SpotLightComponent) const;

    void RenderForLightCommon(ULightComponentBase* LightComponent) const;

    
    void RenderForProjectileMovementComponent(UProjectileMovementComponent* ProjectileComp);
    void RenderForTextComponent(UTextComponent* TextComponent) const;
    
    /* Materials Settings */
    void RenderForMaterial(UStaticMeshComponent* StaticMeshComp);
    void RenderMaterialView(UMaterial* Material);
    void RenderCreateMaterialView();

    void RenderForExponentialHeightFogComponent(UHeightFogComponent* FogComponent);

    void RenderForShapeComponent(UShapeComponent* ShapeComponent);
    void RenderForSpringArmComponent(USpringArmComponent* SpringArmComponent);
    
    template<typename T>
        requires std::derived_from<T, UActorComponent>
    T* GetTargetComponent(AActor* SelectedActor, USceneComponent* SelectedComponent);

    template <class T>
    void RenderProperties(T* Obj);

private:
    float Width = 0, Height = 0;
    FVector Location = FVector(0, 0, 0);
    FRotator Rotation = FRotator(0, 0, 0);
    FVector Scale = FVector(0, 0, 0);
    FVector LightDirection = FVector(0, 0, 0);
    /* Material Property */
    int SelectedMaterialIndex = -1;
    int CurMaterialIndex = -1;
    UStaticMeshComponent* SelectedStaticMeshComp = nullptr;
    FMaterialInfo tempMaterialInfo;
    bool IsCreateMaterial;

    const FString TemplateFilePath = FString("LuaScripts/template.lua");

    /* Animation Property for Debug */
    int SelectedAnimIndex = -1;
    FString SelectedAnimName;

    /* Animation Blend Property for Debug */
    int SelectedAnimIndexA = -1;
    FString SelectedAnimNameA;

    int SelectedAnimIndexB = -1;
    FString SelectedAnimNameB;

};

template <typename T> requires std::derived_from<T, UActorComponent>
T* PropertyEditorPanel::GetTargetComponent(AActor* SelectedActor, USceneComponent* SelectedComponent)
{
    T* ResultComp = nullptr;
    if (SelectedComponent != nullptr)
    {
        ResultComp = Cast<T>(SelectedComponent);
    }
    else if (SelectedActor != nullptr)
    {
        ResultComp = SelectedActor->GetComponentByClass<T>();
    }
        
    return ResultComp;
}

template<typename T>
void PropertyEditorPanel::RenderProperties(T* Obj)
{
    if (!Obj)
    {
        return;
    }
    UObject* UObjectPtr = Cast<UObject>(Obj);
    UClass* Cls = UObjectPtr->GetClass();

    // 등록된 모든 UField 를 순회하면서 DrawFieldEditor 호출
    Cls->ForEachField([&](UField* Field)
        {
            ImGuiInspector::DrawFieldEditor(Field, UObjectPtr);
            ImGui::Spacing();
        });
}
