#include "MathSSE.h"

#ifdef __SSE_EXPERIMENTAL__
// General한 방법으로, Transform에는 사용하지 말것.
// SRT만을 사용한 경우에는 InverseTransformNoScale / InverseTransform 사용하기.
// Transform의 경우에는 determinant가 작아서 FMatrix::Inverse와 결과가 다르게 나옴.
void SSE::InverseMatrix(FMatrix* OutMat, const FMatrix* InMat)
{
    // 레지스터에 값 로드
    const VectorRegister4Float* InMatrixPtr = reinterpret_cast<const VectorRegister4Float*>(InMat);
    VectorRegister4Float* OutMatrixPtr = reinterpret_cast<VectorRegister4Float*>(OutMat);

    // use block matrix method
    // A is a matrix, then i(A) or iA means inverse of A, A# (or A_ in code) means adjugate of A, |A| (or detA in code) is determinant, tr(A) is trace

    // sub matrices
    __m128 A = VecShuffle_0101(InMatrixPtr[0], InMatrixPtr[1]);
    __m128 B = VecShuffle_2323(InMatrixPtr[0], InMatrixPtr[1]);
    __m128 C = VecShuffle_0101(InMatrixPtr[2], InMatrixPtr[3]);
    __m128 D = VecShuffle_2323(InMatrixPtr[2], InMatrixPtr[3]);

    // determinant as (|A| |B| |C| |D|)
    __m128 detSub = _mm_sub_ps(
        _mm_mul_ps(VecShuffle(InMatrixPtr[0], InMatrixPtr[2], 0, 2, 0, 2), VecShuffle(InMatrixPtr[1], InMatrixPtr[3], 1, 3, 1, 3)),
        _mm_mul_ps(VecShuffle(InMatrixPtr[0], InMatrixPtr[2], 1, 3, 1, 3), VecShuffle(InMatrixPtr[1], InMatrixPtr[3], 0, 2, 0, 2))
    );
    __m128 detA = VecSwizzle1(detSub, 0);
    __m128 detB = VecSwizzle1(detSub, 1);
    __m128 detC = VecSwizzle1(detSub, 2);
    __m128 detD = VecSwizzle1(detSub, 3);

    // let iM = 1/|M| * | X  Y |
    //                  | Z  W |

    // D#C
    __m128 D_C = Mat2AdjMul(D, C);
    // A#B
    __m128 A_B = Mat2AdjMul(A, B);
    // X# = |D|A - B(D#C)
    __m128 X_ = _mm_sub_ps(_mm_mul_ps(detD, A), Mat2Mul(B, D_C));
    // W# = |A|D - C(A#B)
    __m128 W_ = _mm_sub_ps(_mm_mul_ps(detA, D), Mat2Mul(C, A_B));

    // |M| = |A|*|D| + ... (continue later)
    __m128 detM = _mm_mul_ps(detA, detD);

    // Y# = |B|C - D(A#B)#
    __m128 Y_ = _mm_sub_ps(_mm_mul_ps(detB, C), Mat2MulAdj(D, A_B));
    // Z# = |C|B - A(D#C)#
    __m128 Z_ = _mm_sub_ps(_mm_mul_ps(detC, B), Mat2MulAdj(A, D_C));

    // |M| = |A|*|D| + |B|*|C| ... (continue later)
    detM = _mm_add_ps(detM, _mm_mul_ps(detB, detC));

    // tr((A#B)(D#C))
    __m128 tr = _mm_mul_ps(A_B, VecSwizzle(D_C, 0, 2, 1, 3));
    tr = _mm_hadd_ps(tr, tr);
    tr = _mm_hadd_ps(tr, tr);
    // |M| = |A|*|D| + |B|*|C| - tr((A#B)(D#C)
    detM = _mm_sub_ps(detM, tr);

    const __m128 adjSignMask = _mm_setr_ps(1.f, -1.f, -1.f, 1.f);
    // (1/|M|, -1/|M|, -1/|M|, 1/|M|)
    __m128 rDetM = _mm_div_ps(adjSignMask, detM);

    X_ = _mm_mul_ps(X_, rDetM);
    Y_ = _mm_mul_ps(Y_, rDetM);
    Z_ = _mm_mul_ps(Z_, rDetM);
    W_ = _mm_mul_ps(W_, rDetM);


    // apply adjugate and store, here we combine adjugate shuffle and store shuffle
    OutMatrixPtr[0] = VecShuffle(X_, Y_, 3, 1, 3, 1);
    OutMatrixPtr[1] = VecShuffle(X_, Y_, 2, 0, 2, 0);
    OutMatrixPtr[2] = VecShuffle(Z_, W_, 3, 1, 3, 1);
    OutMatrixPtr[3] = VecShuffle(Z_, W_, 2, 0, 2, 0);
}

// SRT 변환 행렬 중 Scale=(1,1,1)인 경우에만 사용
void SSE::InverseTransformNoScale(FMatrix* OutMat, const FMatrix* InMat)
{
    // 레지스터에 값 로드
    const VectorRegister4Float* InMatrixPtr = reinterpret_cast<const VectorRegister4Float*>(InMat);
    VectorRegister4Float* OutMatrixPtr = reinterpret_cast<VectorRegister4Float*>(OutMat);

    // transpose 3x3, we know m03 = m13 = m23 = 0
    __m128 t0 = VecShuffle_0101(InMatrixPtr[0], InMatrixPtr[1]); // 00, 01, 10, 11
    __m128 t1 = VecShuffle_2323(InMatrixPtr[0], InMatrixPtr[1]); // 02, 03, 12, 13
    OutMatrixPtr[0] = VecShuffle(t0, InMatrixPtr[2], 0, 2, 0, 3); // 00, 10, 20, 23(=0)
    OutMatrixPtr[1] = VecShuffle(t0, InMatrixPtr[2], 1, 3, 1, 3); // 01, 11, 21, 23(=0)
    OutMatrixPtr[2] = VecShuffle(t1, InMatrixPtr[2], 0, 2, 2, 3); // 02, 12, 22, 23(=0)

    // last line
    OutMatrixPtr[3] = _mm_mul_ps(OutMatrixPtr[0], VecSwizzle1(InMatrixPtr[3], 0));
    OutMatrixPtr[3] = _mm_add_ps(OutMatrixPtr[3], _mm_mul_ps(OutMatrixPtr[1], VecSwizzle1(InMatrixPtr[3], 1)));
    OutMatrixPtr[3] = _mm_add_ps(OutMatrixPtr[3], _mm_mul_ps(OutMatrixPtr[2], VecSwizzle1(InMatrixPtr[3], 2)));
    OutMatrixPtr[3] = _mm_sub_ps(_mm_setr_ps(0.f, 0.f, 0.f, 1.f), OutMatrixPtr[3]);
}

// SRT 변환 행렬에만 사용
void SSE::InverseTransform(FMatrix* OutMat, const FMatrix* InMat)
{
    // 레지스터에 값 로드
    const VectorRegister4Float* InMatrixPtr = reinterpret_cast<const VectorRegister4Float*>(InMat);
    VectorRegister4Float* OutMatrixPtr = reinterpret_cast<VectorRegister4Float*>(OutMat);

    // transpose 3x3, we know m03 = m13 = m23 = 0
    __m128 t0 = VecShuffle_0101(InMatrixPtr[0], InMatrixPtr[1]); // 00, 01, 10, 11
    __m128 t1 = VecShuffle_2323(InMatrixPtr[0], InMatrixPtr[1]); // 02, 03, 12, 13
    OutMatrixPtr[0] = VecShuffle(t0, InMatrixPtr[2], 0, 2, 0, 3); // 00, 10, 20, 23(=0)
    OutMatrixPtr[1] = VecShuffle(t0, InMatrixPtr[2], 1, 3, 1, 3); // 01, 11, 21, 23(=0)
    OutMatrixPtr[2] = VecShuffle(t1, InMatrixPtr[2], 0, 2, 2, 3); // 02, 12, 22, 23(=0)

    // (SizeSqr(mVec[0]), SizeSqr(mVec[1]), SizeSqr(mVec[2]), 0)
    __m128 sizeSqr;
    sizeSqr = _mm_mul_ps(OutMatrixPtr[0], OutMatrixPtr[0]);
    sizeSqr = _mm_add_ps(sizeSqr, _mm_mul_ps(OutMatrixPtr[1], OutMatrixPtr[1]));
    sizeSqr = _mm_add_ps(sizeSqr, _mm_mul_ps(OutMatrixPtr[2], OutMatrixPtr[2]));

    // optional test to avoid divide by 0
    __m128 one = _mm_set1_ps(1.f);
    // for each component, if(sizeSqr < SMALL_NUMBER) sizeSqr = 1;
    __m128 rSizeSqr = _mm_blendv_ps(
        _mm_div_ps(one, sizeSqr),
        one,
        _mm_cmplt_ps(sizeSqr, _mm_set1_ps(1.e-8f))
    );

    OutMatrixPtr[0] = _mm_mul_ps(OutMatrixPtr[0], rSizeSqr);
    OutMatrixPtr[1] = _mm_mul_ps(OutMatrixPtr[1], rSizeSqr);
    OutMatrixPtr[2] = _mm_mul_ps(OutMatrixPtr[2], rSizeSqr);

    // last line
    OutMatrixPtr[3] = _mm_mul_ps(OutMatrixPtr[0], VecSwizzle1(InMatrixPtr[3], 0));
    OutMatrixPtr[3] = _mm_add_ps(OutMatrixPtr[3], _mm_mul_ps(OutMatrixPtr[1], VecSwizzle1(InMatrixPtr[3], 1)));
    OutMatrixPtr[3] = _mm_add_ps(OutMatrixPtr[3], _mm_mul_ps(OutMatrixPtr[2], VecSwizzle1(InMatrixPtr[3], 2)));
    OutMatrixPtr[3] = _mm_sub_ps(_mm_setr_ps(0.f, 0.f, 0.f, 1.f), OutMatrixPtr[3]);
}
#endif // __SSE_EXPERIMENTAL__
