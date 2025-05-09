#pragma once
#include <immintrin.h>
#include "HAL/PlatformType.h"

//#define __SSE_EXPERIMENTAL__

/**
 * @param A0    Selects which element (0-3) from 'A' into 1st slot in the result
 * @param A1    Selects which element (0-3) from 'A' into 2nd slot in the result
 * @param B2    Selects which element (0-3) from 'B' into 3rd slot in the result
 * @param B3    Selects which element (0-3) from 'B' into 4th slot in the result
 */
#define SHUFFLEMASK(A0,A1,B2,B3) ( (A0) | ((A1)<<2) | ((B2)<<4) | ((B3)<<6) )

#define VectorReplicate(Vec, Index) VectorReplicateTemplate<Index>(Vec)

struct FMatrix;
struct FVector4;

// 4 floats
typedef __m128    VectorRegister4Float;


namespace SSE
{
/**
 * Vector의 특정 인덱스를 복제합니다.
 * @tparam Index 복제할 Index (0 ~ 3)
 * @param Vector 복제할 대상
 * @return 복제된 레지스터
 */
template <int Index>
FORCEINLINE VectorRegister4Float VectorReplicateTemplate(const VectorRegister4Float& Vector)
{
    return _mm_shuffle_ps(Vector, Vector, SHUFFLEMASK(Index, Index, Index, Index));
}


/** Vector4 곱연산 */
FORCEINLINE VectorRegister4Float VectorMultiply(const VectorRegister4Float& Vec1, const VectorRegister4Float& Vec2)
{
    return _mm_mul_ps(Vec1, Vec2);
}

FORCEINLINE VectorRegister4Float VectorAdd(const VectorRegister4Float& Vec1, const VectorRegister4Float& Vec2)
{
    return _mm_add_ps(Vec1, Vec2);
}

FORCEINLINE VectorRegister4Float VectorMultiplyAdd(
    const VectorRegister4Float& Vec1,
    const VectorRegister4Float& Vec2,
    const VectorRegister4Float& Vec3
)
{
    return VectorAdd(VectorMultiply(Vec1, Vec2), Vec3);
}

inline void VectorMatrixMultiply(FMatrix* Result, const FMatrix* Matrix1, const FMatrix* Matrix2)
{
    // 레지스터에 값 로드
    const VectorRegister4Float* Matrix1Ptr = reinterpret_cast<const VectorRegister4Float*>(Matrix1);
    const VectorRegister4Float* Matrix2Ptr = reinterpret_cast<const VectorRegister4Float*>(Matrix2);
    VectorRegister4Float* Ret = reinterpret_cast<VectorRegister4Float*>(Result);
    VectorRegister4Float Temp, R0, R1, R2;

    // 첫번째 행 계산
    Temp = VectorMultiply(VectorReplicate(Matrix1Ptr[0], 0), Matrix2Ptr[0]);
    Temp = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[0], 1), Matrix2Ptr[1], Temp);
    Temp = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[0], 2), Matrix2Ptr[2], Temp);
    R0 = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[0], 3), Matrix2Ptr[3], Temp);

    // 두번째 행 계산
    Temp = VectorMultiply(VectorReplicate(Matrix1Ptr[1], 0), Matrix2Ptr[0]);
    Temp = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[1], 1), Matrix2Ptr[1], Temp);
    Temp = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[1], 2), Matrix2Ptr[2], Temp);
    R1 = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[1], 3), Matrix2Ptr[3], Temp);

    // 세번째 행 계산
    Temp = VectorMultiply(VectorReplicate(Matrix1Ptr[2], 0), Matrix2Ptr[0]);
    Temp = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[2], 1), Matrix2Ptr[1], Temp);
    Temp = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[2], 2), Matrix2Ptr[2], Temp);
    R2 = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[2], 3), Matrix2Ptr[3], Temp);

    // 네번째 행 계산
    Temp = VectorMultiply(VectorReplicate(Matrix1Ptr[3], 0), Matrix2Ptr[0]);
    Temp = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[3], 1), Matrix2Ptr[1], Temp);
    Temp = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[3], 2), Matrix2Ptr[2], Temp);
    Temp = VectorMultiplyAdd(VectorReplicate(Matrix1Ptr[3], 3), Matrix2Ptr[3], Temp);

    // 결과 저장
    Ret[0] = R0;
    Ret[1] = R1;
    Ret[2] = R2;
    Ret[3] = Temp;
}

FORCEINLINE void TransformVector4_SSE(FVector4* Result, const FVector4* vector, const FMatrix* mat)
{
    // reinterpret cast로 포인터 변환
    const VectorRegister4Float* VectorPtr = reinterpret_cast<const VectorRegister4Float*>(vector);
    const VectorRegister4Float* MatrixPtr = reinterpret_cast<const VectorRegister4Float*>(mat);
    VectorRegister4Float* Ret = reinterpret_cast<VectorRegister4Float*>(Result);

    const VectorRegister4Float V = *VectorPtr;

    // 각 행(row)에 대해 벡터 곱 + 누적
    // DPPS 마스크 설명:
    // 0xF1 = 결과 x에만 저장, x*y*z*w 모두 곱
    // 0xF2 = 결과 y에만 저장, ...
    // 0xF4 = 결과 z에만 저장
    // 0xF8 = 결과 w에만 저장

    VectorRegister4Float R0 = _mm_dp_ps(MatrixPtr[0], V, 0xF1); // X
    VectorRegister4Float R1 = _mm_dp_ps(MatrixPtr[1], V, 0xF2); // Y
    VectorRegister4Float R2 = _mm_dp_ps(MatrixPtr[2], V, 0xF4); // Z
    VectorRegister4Float R3 = _mm_dp_ps(MatrixPtr[3], V, 0xF8); // W

    // OR 연산으로 병합
    VectorRegister4Float XY = _mm_or_ps(R0, R1);
    VectorRegister4Float ZW = _mm_or_ps(R2, R3);
    *Ret = _mm_or_ps(XY, ZW);
}


// W값은 1로 강제로 변환될것임.
FORCEINLINE void TransformPosition_SSE(FVector4* OutResult, const FVector4* InVector, const FMatrix* Matrix)
{
    // reinterpret cast로 포인터 변환
    const VectorRegister4Float* VectorPtr = reinterpret_cast<const VectorRegister4Float*>(InVector);
    const VectorRegister4Float* MatrixPtr = reinterpret_cast<const VectorRegister4Float*>(Matrix);
    VectorRegister4Float* Ret = reinterpret_cast<VectorRegister4Float*>(OutResult);

    // InVector: {X, Y, Z, ignored} → 강제로 W=1.0 추가
    VectorRegister4Float V = _mm_insert_ps(*VectorPtr, _mm_set_ss(1.0f), 0x30); // 마지막 요소에 1.0 넣기

    VectorRegister4Float R0 = _mm_dp_ps(MatrixPtr[0], V, 0xF1); // X
    VectorRegister4Float R1 = _mm_dp_ps(MatrixPtr[1], V, 0xF2); // Y
    VectorRegister4Float R2 = _mm_dp_ps(MatrixPtr[2], V, 0xF4); // Z
    VectorRegister4Float R3 = _mm_dp_ps(MatrixPtr[3], V, 0xF8); // W

    VectorRegister4Float XY = _mm_or_ps(R0, R1);
    VectorRegister4Float ZW = _mm_or_ps(R2, R3);
    *Ret = _mm_or_ps(XY, ZW);

    // W 분할은 호출자가 원하면 따로 처리
}

FORCEINLINE void TransformNormal_SSE(FVector4* OutResult, const FVector4* InVector, const FMatrix* Matrix)
{
    // reinterpret cast로 포인터 변환
    const VectorRegister4Float* VectorPtr = reinterpret_cast<const VectorRegister4Float*>(InVector);
    const VectorRegister4Float* MatrixPtr = reinterpret_cast<const VectorRegister4Float*>(Matrix);
    VectorRegister4Float* Ret = reinterpret_cast<VectorRegister4Float*>(OutResult);

    // InVector: {X, Y, Z, ignored} → 강제로 W=0.0 설정
    VectorRegister4Float V = _mm_insert_ps(*VectorPtr, _mm_setzero_ps(), 0x30); // W = 0

    VectorRegister4Float R0 = _mm_dp_ps(MatrixPtr[0], V, 0xF1); // X
    VectorRegister4Float R1 = _mm_dp_ps(MatrixPtr[1], V, 0xF2); // Y
    VectorRegister4Float R2 = _mm_dp_ps(MatrixPtr[2], V, 0xF4); // Z

    VectorRegister4Float XY = _mm_or_ps(R0, R1);
    *Ret = _mm_or_ps(XY, R2);
}



//////////////////////////////////
// Inverse Matrix 테스트용
#ifdef __SSE_EXPERIMENTAL__
#define MakeShuffleMask(x,y,z,w)           (x | (y<<2) | (z<<4) | (w<<6))

 // vec(0, 1, 2, 3) -> (vec[x], vec[y], vec[z], vec[w])
#define VecSwizzleMask(vec, mask)          _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(vec), mask))
#define VecSwizzle(vec, x, y, z, w)        VecSwizzleMask(vec, MakeShuffleMask(x,y,z,w))
#define VecSwizzle1(vec, x)                VecSwizzleMask(vec, MakeShuffleMask(x,x,x,x))
// special swizzle
#define VecSwizzle_0022(vec)               _mm_moveldup_ps(vec)
#define VecSwizzle_1133(vec)               _mm_movehdup_ps(vec)

#define VecShuffle(vec1, vec2, x,y,z,w)    _mm_shuffle_ps(vec1, vec2, MakeShuffleMask(x,y,z,w))
// special shuffle
#define VecShuffle_0101(vec1, vec2)        _mm_movelh_ps(vec1, vec2)
#define VecShuffle_2323(vec1, vec2)        _mm_movehl_ps(vec2, vec1)

    // https://lxjk.github.io/2017/09/03/Fast-4x4-Matrix-Inverse-with-SSE-SIMD-Explained.html
    __forceinline __m128 Mat2Mul(__m128 vec1, __m128 vec2)
    {
        return
            _mm_add_ps(_mm_mul_ps(vec1, VecSwizzle(vec2, 0, 3, 0, 3)),
                _mm_mul_ps(VecSwizzle(vec1, 1, 0, 3, 2), VecSwizzle(vec2, 2, 1, 2, 1)));
    }
    // 2x2 row major Matrix adjugate multiply (A#)*B
    __forceinline __m128 Mat2AdjMul(__m128 vec1, __m128 vec2)
    {
        return
            _mm_sub_ps(_mm_mul_ps(VecSwizzle(vec1, 3, 3, 0, 0), vec2),
                _mm_mul_ps(VecSwizzle(vec1, 1, 1, 2, 2), VecSwizzle(vec2, 2, 3, 0, 1)));

    }
    // 2x2 row major Matrix multiply adjugate A*(B#)
    __forceinline __m128 Mat2MulAdj(__m128 vec1, __m128 vec2)
    {
        return
            _mm_sub_ps(_mm_mul_ps(vec1, VecSwizzle(vec2, 3, 0, 3, 0)),
                _mm_mul_ps(VecSwizzle(vec1, 1, 0, 3, 2), VecSwizzle(vec2, 2, 1, 2, 1)));
    }


    // General한 방법으로, Transform에는 사용하지 말것.
    // SRT만을 사용한 경우에는 InverseTransformNoScale / InverseTransform 사용하기.
    // Transform의 경우에는 determinant가 작아서 FMatrix::Inverse와 결과가 다르게 나옴.
    void InverseMatrix(FMatrix* OutMat, const FMatrix* InMat);

    // SRT 변환 행렬 중 Scale=(1,1,1)인 경우에만 사용
    void InverseTransformNoScale(FMatrix* OutMat, const FMatrix* InMat);

    // SRT 변환 행렬에만 사용
    void InverseTransform(FMatrix* OutMat, const FMatrix* InMat);
#else
#endif
// Inverse Matrix 테스트용
//////////////////////////////////


FORCEINLINE float TruncToFloat(float F)
{
    return _mm_cvtss_f32(_mm_round_ps(_mm_set_ss(F), 3));
}

FORCEINLINE double TruncToDouble(double F)
{
    return _mm_cvtsd_f64(_mm_round_pd(_mm_set_sd(F), 3));
}

FORCEINLINE float FloorToFloat(float F)
{
    return _mm_cvtss_f32(_mm_floor_ps(_mm_set_ss(F)));
}

FORCEINLINE double FloorToDouble(double F)
{
    return _mm_cvtsd_f64(_mm_floor_pd(_mm_set_sd(F)));
}

FORCEINLINE float RoundToFloat(float F)
{
    return FloorToFloat(F + 0.5f);
}

FORCEINLINE double RoundToDouble(double F)
{
    return FloorToDouble(F + 0.5);
}

FORCEINLINE float CeilToFloat(float F)
{
    return _mm_cvtss_f32(_mm_ceil_ps(_mm_set_ss(F)));
}

FORCEINLINE double CeilToDouble(double F)
{
    return _mm_cvtsd_f64(_mm_ceil_pd(_mm_set_sd(F)));
}
}
