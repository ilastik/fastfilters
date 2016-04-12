// fastfilters
// Copyright (c) 2016 Sven Peter
// sven.peter@iwr.uni-heidelberg.de or mail@svenpeter.me
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include "fastfilters.h"
#include "common.h"
#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define FASTFILTERS_FIR_CONVOLVE_NOSIMD_IMPL_H

// hack to make waf dependency tracking + warning generation work correctly
#define FF_NOP
#include "fir_convolve_nosimd_impl.h"
#undef FF_NOP

#define FF_UNROLL 20

// yo dawg, i herd you like #includes so i put an #include in your #include so you can #include while you #include
#include <boost/preprocessor/iteration/iterate.hpp>
#include <boost/preprocessor/repetition/enum.hpp>
#include <boost/preprocessor/list/cat.hpp>

#define BOOST_PP_ITERATION_LIMITS (1, FF_UNROLL)
#define BOOST_PP_FILENAME_1 "fir_convolve_nosimd_impl.h"
#include BOOST_PP_ITERATE()
#undef BOOST_PP_ITERATION_LIMITS
#undef BOOST_PP_FILENAME_1

typedef bool (*impl_fn_t)(const float *, size_t, size_t, size_t, size_t, float *, const float *);

#define IMPL_FNAME(z, n, text) &BOOST_PP_CAT(BOOST_PP_CAT(fir_convolve_impl_, text), BOOST_PP_INC(n))
#define IMPL_OUTER_FNAME(z, n, text) &BOOST_PP_CAT(BOOST_PP_CAT(fir_convolve_outer_impl_, text), BOOST_PP_INC(n))

#define TBLNAME_SYMMETRIC(x) BOOST_PP_IF(x, symmetric_, antisymmetric_)
#define TBLNAME_SYMMETRIC2(x) BOOST_PP_IF(x, symmetric, antisymmetric)
#define TBLNAME_BORDER(x) BOOST_PP_IF(x, optimistic_, mirror_)
#define TBLNAME_BORDER2(x) BOOST_PP_IF(x, optimistic, mirror)

#define DEFINE_JMPTBL(outer, left_border, right_border, symmetric)                                                     \
    static impl_fn_t BOOST_PP_CAT(                                                                                     \
        BOOST_PP_IF(outer, impl_fn_outer_, impl_fn_),                                                                  \
        BOOST_PP_CAT(TBLNAME_SYMMETRIC(symmetric),                                                                     \
                     BOOST_PP_CAT(TBLNAME_BORDER(left_border), TBLNAME_BORDER2(right_border))))[] = {                  \
        BOOST_PP_ENUM(FF_UNROLL, BOOST_PP_IF(outer, IMPL_OUTER_FNAME, IMPL_FNAME),                                     \
                      BOOST_PP_CAT(BOOST_PP_CAT(TBLNAME_BORDER(left_border), TBLNAME_BORDER(right_border)),            \
                                   TBLNAME_SYMMETRIC2(symmetric)))};

DEFINE_JMPTBL(0, 0, 0, 0);
DEFINE_JMPTBL(0, 0, 0, 1);
DEFINE_JMPTBL(0, 0, 1, 0);
DEFINE_JMPTBL(0, 0, 1, 1);
DEFINE_JMPTBL(0, 1, 0, 0);
DEFINE_JMPTBL(0, 1, 0, 1);
DEFINE_JMPTBL(0, 1, 1, 0);
DEFINE_JMPTBL(0, 1, 1, 1);
DEFINE_JMPTBL(1, 0, 0, 0);
DEFINE_JMPTBL(1, 0, 0, 1);
DEFINE_JMPTBL(1, 0, 1, 0);
DEFINE_JMPTBL(1, 0, 1, 1);
DEFINE_JMPTBL(1, 1, 0, 0);
DEFINE_JMPTBL(1, 1, 0, 1);
DEFINE_JMPTBL(1, 1, 1, 0);
DEFINE_JMPTBL(1, 1, 1, 1);

struct impl_fn_jmptbl_selection {
    impl_fn_t *jmptbl;
    fastfilters_border_treatment_t left_border;
    fastfilters_border_treatment_t right_border;
    bool is_symmetric;
};

#define DEFINE_JMPTBL_STRUCT(outer, x, y, symmetric)                                                                   \
    {                                                                                                                  \
        .jmptbl = BOOST_PP_CAT(                                                                                        \
            BOOST_PP_IF(outer, impl_fn_outer_, impl_fn_),                                                              \
            BOOST_PP_CAT(TBLNAME_SYMMETRIC(symmetric), BOOST_PP_CAT(TBLNAME_BORDER(x), TBLNAME_BORDER2(y)))),          \
        .left_border = BOOST_PP_IF(x, FASTFILTERS_BORDER_OPTIMISTIC, FASTFILTERS_BORDER_MIRROR),                       \
        .right_border = BOOST_PP_IF(y, FASTFILTERS_BORDER_OPTIMISTIC, FASTFILTERS_BORDER_MIRROR),                      \
        .is_symmetric = BOOST_PP_IF(symmetric, true, false)                                                            \
    }

static const struct impl_fn_jmptbl_selection impl_fn_tbls_inner[] = {
    DEFINE_JMPTBL_STRUCT(0, 0, 0, 0), DEFINE_JMPTBL_STRUCT(0, 0, 0, 1), DEFINE_JMPTBL_STRUCT(0, 0, 1, 0),
    DEFINE_JMPTBL_STRUCT(0, 0, 1, 1), DEFINE_JMPTBL_STRUCT(0, 1, 0, 0), DEFINE_JMPTBL_STRUCT(0, 1, 0, 1),
    DEFINE_JMPTBL_STRUCT(0, 1, 1, 0), DEFINE_JMPTBL_STRUCT(0, 1, 1, 1)};

static const struct impl_fn_jmptbl_selection impl_fn_tbls_outer[] = {
    DEFINE_JMPTBL_STRUCT(1, 0, 0, 0), DEFINE_JMPTBL_STRUCT(1, 0, 0, 1), DEFINE_JMPTBL_STRUCT(1, 0, 1, 0),
    DEFINE_JMPTBL_STRUCT(1, 0, 1, 1), DEFINE_JMPTBL_STRUCT(1, 1, 0, 0), DEFINE_JMPTBL_STRUCT(1, 1, 0, 1),
    DEFINE_JMPTBL_STRUCT(1, 1, 1, 0), DEFINE_JMPTBL_STRUCT(1, 1, 1, 1)};

#define ARRAY_LENGTH(x) (sizeof((x)) / sizeof((x)[0]))

bool fastfilters_fir_convolve_fir_inner(const float *inptr, size_t n_pixels, size_t pixel_stride, size_t n_outer,
                                        size_t outer_stride, float *outptr, fastfilters_kernel_fir_t kernel,
                                        fastfilters_border_treatment_t left_border,
                                        fastfilters_border_treatment_t right_border)
{
    if (kernel->len == 0)
        return false;

    if (kernel->len > FF_UNROLL)
        return false;

    impl_fn_t *jmptbl = NULL;

    for (unsigned int i = 0; i < ARRAY_LENGTH(impl_fn_tbls_inner); ++i) {
        if (left_border != impl_fn_tbls_inner[i].left_border)
            continue;
        if (right_border != impl_fn_tbls_inner[i].right_border)
            continue;
        if (kernel->is_symmetric != impl_fn_tbls_inner[i].is_symmetric)
            continue;
        jmptbl = impl_fn_tbls_inner[i].jmptbl;
        break;
    }

    if (jmptbl == NULL)
        return false;

    return jmptbl[kernel->len - 1](inptr, n_pixels, pixel_stride, n_outer, outer_stride, outptr, kernel->coefs);
}

bool fastfilters_fir_convolve_fir_outer(const float *inptr, size_t n_pixels, size_t pixel_stride, size_t n_outer,
                                        size_t outer_stride, float *outptr, fastfilters_kernel_fir_t kernel,
                                        fastfilters_border_treatment_t left_border,
                                        fastfilters_border_treatment_t right_border)
{
    if (kernel->len == 0)
        return false;

    if (kernel->len > FF_UNROLL)
        return false;

    impl_fn_t *jmptbl = NULL;

    for (unsigned int i = 0; i < ARRAY_LENGTH(impl_fn_tbls_outer); ++i) {
        if (left_border != impl_fn_tbls_outer[i].left_border)
            continue;
        if (right_border != impl_fn_tbls_outer[i].right_border)
            continue;
        if (kernel->is_symmetric != impl_fn_tbls_outer[i].is_symmetric)
            continue;
        jmptbl = impl_fn_tbls_outer[i].jmptbl;
        break;
    }

    if (jmptbl == NULL)
        return false;

    return jmptbl[kernel->len - 1](inptr, n_pixels, pixel_stride, n_outer, outer_stride, outptr, kernel->coefs);
}
