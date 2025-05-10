#pragma once
#include "Animation/AnimCurveTypes.h"
#include "Misc/FrameRate.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"

struct FReferenceSkeleton;
struct FTransform;

class UAnimDataModel : public UObject
{
    DECLARE_CLASS(UAnimDataModel, UObject)
public:
    UAnimDataModel();
    virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const;

    // FBX Loader에서 로드한 애니메이션 Raw Data
    TArray<FBoneAnimationTrack> BoneAnimationTracks;
    float PlayLength;
    FFrameRate FrameRate;
    int32 NumberOfFrames;
    int32 NumberOfKeys;


    FAnimationCurveData CurveData;

    /* 특정 시간의 포즈를 계산하여 OutPose에 채운 후 반환
     * @param OutPose : 로컬 스페이스 본 트랜스폼 배열
     */
    void GetPoseAtTime(float Time, TArray<FTransform>& OutPose, const FReferenceSkeleton& RefSkeleton, bool bLooping) const;

private:
    /* 특정 트랙에서 특정 프레임의 트랜스폼을 가져옴 */
    FTransform GetTransformAtFrame(const FRawAnimSequenceTrack& TrackData, int32 FrameIndex) const;
    /* 두 프레임 사이의 트랜스폼을 보간 */
    FTransform InterpolateTransform(const FTransform& Start, const FTransform& End, float Alpha) const;
};
