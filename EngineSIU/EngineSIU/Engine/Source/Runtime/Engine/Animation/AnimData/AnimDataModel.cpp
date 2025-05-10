#include "AnimDataModel.h"

#include "Engine/Asset/SkeletalMeshAsset.h"
#include "Math/Transform.h"

UAnimDataModel::UAnimDataModel()
    : PlayLength(0.f)
    , NumberOfFrames(0)
{
    // FrameRate 기본값 설정 (예: 30fps)
    FrameRate.Numerator = 30;
    FrameRate.Denominator = 1;
}

const TArray<FBoneAnimationTrack>& UAnimDataModel::GetBoneAnimationTracks() const
{
    return BoneAnimationTracks;
}

void UAnimDataModel::GetPoseAtTime(float Time, TArray<FTransform>& OutPose, const FReferenceSkeleton& RefSkeleton, bool bLooping) const
{
    const int32 NumBones = RefSkeleton.GetRawBoneNum();
    if (OutPose.Num() != NumBones)
    {
        OutPose.SetNum(NumBones); // OutPose 크기를 스켈레톤 본 개수에 맞춤
    }

    // 애니메이션 데이터가 없거나 재생 불가한 상태이면 참조 포즈 반환
    if (BoneAnimationTracks.IsEmpty() || NumberOfFrames == 0 || PlayLength <= 0.f || FrameRate.AsDecimal() <= 0.f)
    {
        const TArray<FTransform>& RefBonePose = RefSkeleton.RawRefBonePose;
        for (int32 i = 0; i < NumBones; ++i)
        {
            if (RefBonePose.IsValidIndex(i)) // 참조 포즈 배열 크기 확인
            {
                OutPose[i] = RefBonePose[i];
            }
            else // 참조 포즈 정보가 부족하면 Identity 설정 (이런 경우는 거의 없음.. 예외 처리용)
            {
                OutPose[i] = FTransform::Identity;
            }
        }
        return;
    }

    // 현재 시간을 프레임 단위로 변환 (0부터 NumberOfFrames-1까지의 범위)
    float CurrentFrameAsFloat = Time * FrameRate.AsDecimal();

    if (bLooping)
    {
        if (NumberOfFrames > 0) // Div By Zero 방지
        { 
            CurrentFrameAsFloat = FMath::Fmod(CurrentFrameAsFloat, (float)NumberOfFrames);
            if (CurrentFrameAsFloat < 0.0f) // Fmod 결과가 음수일 수 있음
            {
                CurrentFrameAsFloat += (float)NumberOfFrames;
            }
        }
        else 
        {
            CurrentFrameAsFloat = 0.0f;
        }
    }
    else
    {
        // 마지막 프레임 포함을 위해 약간의 오차 허용
        CurrentFrameAsFloat = FMath::Clamp(CurrentFrameAsFloat, 0.f, (float)NumberOfFrames - 1.00001f); 
    }

    // 보간을 위한 두 프레임 인덱스와 알파 값 계산
    int32 Frame1Idx = FMath::FloorToInt32(CurrentFrameAsFloat);
    int32 Frame2Idx = FMath::CeilToInt(CurrentFrameAsFloat);
    float Alpha = CurrentFrameAsFloat - (float)Frame1Idx;

    // 프레임 인덱스가 유효한 범위 내에 있도록 보장
    // (NumberOfFrames는 키의 개수. 유효 인덱스는 0 ~ NumberOfFrames-1)
    Frame1Idx = FMath::Clamp(Frame1Idx, 0, NumberOfFrames - 1);
    if (bLooping && Frame2Idx >= NumberOfFrames)
    {
        Frame2Idx = 0; // 루프 시 다음 프레임 = 첫 번째 프레임
    }
    else
    {
        Frame2Idx = FMath::Clamp(Frame2Idx, 0, NumberOfFrames - 1);
    }

    // 각 본에 대해 트랜스폼 계산
    for (int32 BoneTreeIdx = 0; BoneTreeIdx < NumBones; ++BoneTreeIdx)
    {
        // 해당 Bone에 대한 애니메이션 트랙 찾기
        const FBoneAnimationTrack* FoundTrack = nullptr;
        for (const FBoneAnimationTrack& Track : BoneAnimationTracks)
        {
            if (Track.BoneTreeIndex == BoneTreeIdx)
            {
                FoundTrack = &Track;
                break;
            }
        }

        if (FoundTrack && !FoundTrack->InternalTrackData.PosKeys.IsEmpty()) // 트랙이 있고, 키 데이터가 있는 경우)
        {
            // 두 프레임에서 트랜스폼 가져오기
            FTransform Transform1 = GetTransformAtFrame(FoundTrack->InternalTrackData, Frame1Idx);
            FTransform Transform2 = GetTransformAtFrame(FoundTrack->InternalTrackData, Frame2Idx);

            // 보간하여 최종 트랜스폼 계산
            OutPose[BoneTreeIdx] = InterpolateTransform(Transform1, Transform2, Alpha);
        }
        else // 해당 본에 대한 애니메이션 트랙이 없거나 키 데이터가 없으면 참조 포즈 사용
        {
            if (RefSkeleton.RawRefBonePose.IsValidIndex(BoneTreeIdx))
            {
                OutPose[BoneTreeIdx] = RefSkeleton.RawRefBonePose[BoneTreeIdx];
            }
            else
            {
                OutPose[BoneTreeIdx] = FTransform::Identity; // 안전장치
            }
        }
    }
}

FTransform UAnimDataModel::GetTransformAtFrame(const FRawAnimSequenceTrack& TrackData, int32 FrameIndex) const
{
    if (NumberOfFrames == 0) return FTransform::Identity; // 안전장치

    int32 SafePosIdx = (TrackData.PosKeys.Num() == 1) ? 0 : FMath::Clamp(FrameIndex, 0, TrackData.PosKeys.Num() - 1);
    int32 SafeRotIdx = (TrackData.RotKeys.Num() == 1) ? 0 : FMath::Clamp(FrameIndex, 0, TrackData.RotKeys.Num() - 1);
    int32 SafeScaleIdx = (TrackData.ScaleKeys.Num() == 1) ? 0 : FMath::Clamp(FrameIndex, 0, TrackData.ScaleKeys.Num() - 1);

    FVector Pos = FVector::ZeroVector;
    FQuat Rot = FQuat::Identity;
    FVector Scale = FVector::OneVector;

    if (TrackData.PosKeys.IsValidIndex(SafePosIdx))
    {
        Pos = TrackData.PosKeys[SafePosIdx];
    }
    if (TrackData.RotKeys.IsValidIndex(SafeRotIdx))
    {
        Rot = TrackData.RotKeys[SafeRotIdx];
    }
    if (TrackData.ScaleKeys.IsValidIndex(SafeScaleIdx))
    {
        Scale = TrackData.ScaleKeys[SafeScaleIdx];
    }

    return FTransform(Pos, Rot, Scale);

}

FTransform UAnimDataModel::InterpolateTransform(const FTransform& Start, const FTransform& End, float Alpha) const
{
    if (Alpha <= 0.0f) return Start;
    if (Alpha >= 1.0f) return End;

    FTransform Result;
    // 위치는 선형 보간 (Lerp)
    Result.SetTranslation(FMath::Lerp(Start.GetTranslation(), End.GetTranslation(), Alpha));
    // 회전은 구면 선형 보간 (Slerp)
    Result.SetRotation(FQuat::Slerp(Start.GetRotation(), End.GetRotation(), Alpha));
    // 스케일도 선형 보간 (Lerp)
    Result.SetScale3D(FMath::Lerp(Start.GetScale3D(), End.GetScale3D(), Alpha));

    Result.NormalizeRotation(); // Slerp 후 정규화 권장
    return Result;
}
