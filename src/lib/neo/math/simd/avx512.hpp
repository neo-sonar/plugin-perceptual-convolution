#pragma once

#include <neo/config.hpp>

#include <immintrin.h>  // AVX-512F

namespace neo::simd {

NEO_ALWAYS_INLINE auto cadd(__m512 a, __m512 b) noexcept -> __m512 { return _mm512_add_ps(a, b); }

NEO_ALWAYS_INLINE auto csub(__m512 a, __m512 b) noexcept -> __m512 { return _mm512_sub_ps(a, b); }

NEO_ALWAYS_INLINE auto cmul(__m512 a, __m512 b) noexcept -> __m512
{
    // Get real part of a
    __m512 ar = _mm512_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 0, 0));

    // Get imaginary part of a
    __m512 ai = _mm512_shuffle_ps(a, a, _MM_SHUFFLE(3, 3, 1, 1));

    // Get real part of b
    __m512 br = _mm512_shuffle_ps(b, b, _MM_SHUFFLE(2, 2, 0, 0));

    // Get imaginary part of b
    __m512 bi = _mm512_shuffle_ps(b, b, _MM_SHUFFLE(3, 3, 1, 1));

    __m512 real = _mm512_mul_ps(ar, br);  // Multiply real parts
    __m512 imag = _mm512_mul_ps(ai, bi);  // Multiply imaginary parts

    // Subtract and add real and imaginary parts
    __m512 rr = _mm512_sub_ps(real, imag);
    __m512 ri = _mm512_add_ps(_mm512_mul_ps(ar, bi), _mm512_mul_ps(ai, br));

    // Interleave real and imaginary parts
    __m512 result = _mm512_shuffle_ps(rr, ri, _MM_SHUFFLE(2, 0, 2, 0));

    return result;
}

NEO_ALWAYS_INLINE auto cadd(__m512d a, __m512d b) noexcept -> __m512d { return _mm512_add_pd(a, b); }

NEO_ALWAYS_INLINE auto csub(__m512d a, __m512d b) noexcept -> __m512d { return _mm512_sub_pd(a, b); }

NEO_ALWAYS_INLINE auto cmul(__m512d a, __m512d b) noexcept -> __m512d
{
    // Get real part of a
    __m512d ar = _mm512_shuffle_pd(a, a, _MM_SHUFFLE2(0, 0));

    // Get imaginary part of a
    __m512d ai = _mm512_shuffle_pd(a, a, _MM_SHUFFLE2(3, 3));

    // Get real part of b
    __m512d br = _mm512_shuffle_pd(b, b, _MM_SHUFFLE2(0, 0));

    // Get imaginary part of b
    __m512d bi = _mm512_shuffle_pd(b, b, _MM_SHUFFLE2(3, 3));

    __m512d real = _mm512_mul_pd(ar, br);  // Multiply real parts
    __m512d imag = _mm512_mul_pd(ai, bi);  // Multiply imaginary parts

    // Subtract and add real and imaginary parts
    __m512d rr = _mm512_sub_pd(real, imag);
    __m512d ri = _mm512_add_pd(_mm512_mul_pd(ar, bi), _mm512_mul_pd(ai, br));

    // Interleave real and imaginary parts
    __m512d result = _mm512_shuffle_pd(rr, ri, _MM_SHUFFLE2(2, 0));

    return result;
}

struct float32x16
{
    using value_type    = float;
    using register_type = __m512;

    static constexpr auto const alignment  = sizeof(register_type);
    static constexpr auto const batch_size = std::size_t(16);

    float32x16() = default;

    float32x16(register_type val) : _val{val} {}

    [[nodiscard]] operator register_type() const { return _val; }

    [[nodiscard]] static auto broadcast(float val) noexcept -> float32x16 { return _mm512_set1_ps(val); }

    auto store_unaligned(float* output) const noexcept -> void { return _mm512_storeu_ps(output, _val); }

private:
    register_type _val;
};

struct float64x8
{
    using value_type    = double;
    using register_type = __m512d;

    static constexpr auto const alignment  = sizeof(register_type);
    static constexpr auto const batch_size = std::size_t(8);

    float64x8() = default;

    float64x8(register_type val) : _val{val} {}

    [[nodiscard]] operator register_type() const { return _val; }

    [[nodiscard]] static auto broadcast(double val) noexcept -> float64x8 { return _mm512_set1_pd(val); }

    auto store_unaligned(double* output) const noexcept -> void { return _mm512_storeu_pd(output, _val); }

private:
    register_type _val;
};

}  // namespace neo::simd
