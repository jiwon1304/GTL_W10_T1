
#include "DamageCameraShake.h"

#include "Camera/PerlinNoiseCameraShakePattern.h"
#include "Camera/Shakes/WaveOscillatorCameraShakePattern.h"

UDamageCameraShake::UDamageCameraShake()
{
    //UWaveOscillatorCameraShakePattern* Pattern = new UWaveOscillatorCameraShakePattern();
    UPerlinNoiseCameraShakePattern* Pattern = new UPerlinNoiseCameraShakePattern();
    Pattern->Duration = 0.5f;
    Pattern->BlendInTime = 0.f;
    Pattern->BlendOutTime = 0.f;

    // Rotation
    Pattern->RotationAmplitudeMultiplier = 1.f;
    Pattern->RotationFrequencyMultiplier = 1.f;
    
    Pattern->Yaw.Amplitude = 3.f;
    Pattern->Yaw.Frequency = 5.f;

    Pattern->Pitch.Amplitude = 5.f;
    Pattern->Pitch.Frequency = 4.f;

    Pattern->Roll.Amplitude = 3.f;
    Pattern->Roll.Frequency = 10.f;

    // Location
    Pattern->LocationAmplitudeMultiplier = 0.f;
    Pattern->LocationFrequencyMultiplier = 1.f;

    Pattern->X.Amplitude = 0.f;
    Pattern->X.Frequency = 1.f;

    Pattern->Y.Amplitude = 5.f;
    Pattern->Y.Frequency = 2.f;

    Pattern->Z.Amplitude = 0.f;
    Pattern->Z.Frequency = 5.f;
    
    SetRootShakePattern(Pattern);
}
