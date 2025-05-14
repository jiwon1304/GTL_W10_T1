#pragma once
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h" 
#include "Classes/Components/InputComponent.h"

class APlayerCameraManager;

class APlayerController : public AActor
{
    DECLARE_CLASS(APlayerController, AActor)
    
public:
    APlayerController();
    ~APlayerController();

    virtual void PostSpawnInitialize() override;

    virtual void BeginPlay();

    virtual void Tick(float DeltaTime) override;
    void ProcessInput(float DeltaTime) const;

    virtual void Destroyed();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

    UInputComponent* GetInputComponent() const { return InputComponent; }

    void SetViewTarget(class AActor* NewViewTarget, struct FViewTargetTransitionParams TransitionParams);

    virtual void Possess(AActor* InActor);

    virtual void UnPossess();
    
    virtual void BindAction(const FString& Key, const std::function<void(float)>& Callback);

    AActor* GetPossessedActor() { return PossessedActor; }
    // 카메라 관련 함수
public:
    void FOV(float NewFOV);
    AActor* GetViewTarget() const;

    virtual void SpawnPlayerCameraManager();

    void ClientStartCameraShake(UClass* Shake);

    void ClientStopCameraShake(UClass* Shake, bool bImmediately = true);

    APlayerCameraManager* PlayerCameraManager = nullptr;
    
protected:
    UPROPERTY
    (None, UInputComponent*, InputComponent, = nullptr)

    virtual void SetupInputComponent();

    AActor* PossessedActor = nullptr;

    bool bHasPossessed = false;
};

