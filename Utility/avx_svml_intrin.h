#include "svml_prolog.h"


__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_sincos_ps(__m256* param0, __m256 param1)
{
    register __m256 reg0 asm("ymm0") = param1;
    register __m256 reg1 asm("ymm1");
    asm(
        "call __vdecl_sincosf8 \t\n"
        : "=v" (reg0), "=v" (reg1)
        : "0" (reg0)
        : "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    *param0 = reg1;
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_sin_ps(__m256 param0)
{
    register __m256 reg0 asm("ymm0") = param0;
    asm(
        "call __vdecl_sinf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0)
        : "%ymm1", "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_cos_ps(__m256 param0)
{
    register __m256 reg0 asm("ymm0") = param0;
    asm(
        "call __vdecl_cosf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0)
        : "%ymm1", "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_tan_ps(__m256 param0)
{
    register __m256 reg0 asm("ymm0") = param0;
    asm(
        "call __vdecl_tanf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0)
        : "%ymm1", "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_asin_ps(__m256 param0)
{
    register __m256 reg0 asm("ymm0") = param0;
    asm(
        "call __vdecl_asinf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0)
        : "%ymm1", "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_acos_ps(__m256 param0)
{
    register __m256 reg0 asm("ymm0") = param0;
    asm(
        "call __vdecl_acosf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0)
        : "%ymm1", "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_atan_ps(__m256 param0)
{
    register __m256 reg0 asm("ymm0") = param0;
    asm(
        "call __vdecl_atanf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0)
        : "%ymm1", "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_log_ps(__m256 param0)
{
    register __m256 reg0 asm("ymm0") = param0;
    asm(
        "call __vdecl_logf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0)
        : "%ymm1", "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_exp_ps(__m256 param0)
{
    register __m256 reg0 asm("ymm0") = param0;
    asm(
        "call __vdecl_expf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0)
        : "%ymm1", "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_exp2_ps(__m256 param0)
{
    register __m256 reg0 asm("ymm0") = param0;
    asm(
        "call __vdecl_exp2f8 \t\n"
        : "=v" (reg0)
        : "0" (reg0)
        : "%ymm1", "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_pow_ps(__m256 param0, __m256 param1)
{
    register __m256 reg0 asm("ymm0") = param0;
    register __m256 reg1 asm("ymm1") = param1;
    asm(
        "call __vdecl_powf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0), "v" (reg1)
        : "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_fmod_ps(__m256 param0, __m256 param1)
{
    register __m256 reg0 asm("ymm0") = param0;
    register __m256 reg1 asm("ymm1") = param1;
    asm(
        "call __vdecl_fmodf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0), "v" (reg1)
        : "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_svml_sqrt_ps(__m256 param0)
{
    register __m256 reg0 asm("ymm0") = param0;
    asm(
        "call __vdecl_sqrtf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0)
        : "%ymm1", "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m256 __DEFAULT_SVML_FN_ATTRS256
_mm256_invsqrt_ps(__m256 param0)
{
    register __m256 reg0 asm("ymm0") = param0;
    asm(
        "call __vdecl_invsqrtf8 \t\n"
        : "=v" (reg0)
        : "0" (reg0)
        : "%ymm1", "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}

__SVML_INTRIN_PROLOG __m128i __DEFAULT_SVML_FN_ATTRS128
_mm_div_epu32(__m128i param0, __m128i param1)
{
    register __m128i reg0 asm("xmm0") = param0;
    register __m128i reg1 asm("xmm1") = param1;
    asm(
        "call __vdecl_u32div4 \t\n"
        : "=v" (reg0)
        : "0" (reg0), "v" (reg1)
        : "%ymm2", "%ymm3", "%ymm4", "%ymm5", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11"
    );
    return reg0;
}