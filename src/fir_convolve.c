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

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "fastfilters.h"
#include "common.h"

typedef bool (*fir_convolve_fn_t)(const float *, size_t, size_t, size_t, size_t, float *, size_t,
                                  fastfilters_kernel_fir_t, fastfilters_border_treatment_t,
                                  fastfilters_border_treatment_t, const float *, const float *, size_t);

static fir_convolve_fn_t g_convolve_inner = NULL;
static fir_convolve_fn_t g_convolve_outer = NULL;

void fastfilters_fir_init(void)
{
    if (fastfilters_cpu_check(FASTFILTERS_CPU_FMA)) {
        g_convolve_outer = &fastfilters_fir_convolve_fir_outer_avxfma;
        g_convolve_inner = &fastfilters_fir_convolve_fir_inner_avxfma;
    } else if (fastfilters_cpu_check(FASTFILTERS_CPU_AVX)) {
        g_convolve_outer = &fastfilters_fir_convolve_fir_outer_avx;
        g_convolve_inner = &fastfilters_fir_convolve_fir_inner_avx;
    } else {
        g_convolve_outer = &fastfilters_fir_convolve_fir_outer;
        g_convolve_inner = &fastfilters_fir_convolve_fir_inner;
    }
}

bool DLL_PUBLIC fastfilters_fir_convolve2d(const fastfilters_array2d_t *inarray, const fastfilters_kernel_fir_t kernelx,
                                           const fastfilters_kernel_fir_t kernely,
                                           const fastfilters_array2d_t *outarray, size_t x0, size_t y0, size_t x1,
                                           size_t y1)
{
    if (x0 == 0 && y0 == 0 && x1 == 0 && y1 == 0) {
        for (unsigned int c = 0; c < inarray->n_channels; ++c) {
            if (!g_convolve_inner(inarray->ptr + c, inarray->n_x, inarray->stride_x, inarray->n_y, inarray->stride_y,
                                  outarray->ptr + c, outarray->stride_y, kernelx, FASTFILTERS_BORDER_MIRROR,
                                  FASTFILTERS_BORDER_MIRROR, NULL, NULL, 0))
                return false;
        }

        return g_convolve_outer(outarray->ptr, inarray->n_y, inarray->stride_y, inarray->n_x * inarray->n_channels,
                                inarray->stride_x / inarray->n_channels, outarray->ptr, outarray->stride_y, kernely,
                                FASTFILTERS_BORDER_MIRROR, FASTFILTERS_BORDER_MIRROR, NULL, NULL, 0);
    }

    if (unlikely(x1 <= x0))
        return false;
    if (unlikely(y1 <= y0))
        return false;

    const size_t n_x = x1 - x0;
    const size_t n_y = y1 - y0;
    bool result = false;

    float *border_top = fastfilters_memory_alloc(sizeof(float) * n_x * inarray->n_channels * kernely->len);
    if (!border_top)
        return false;

    float *border_bottom = fastfilters_memory_alloc(sizeof(float) * n_x * inarray->n_channels * kernely->len);
    if (!border_bottom) {
        fastfilters_memory_free(border_top);
        return false;
    }

    // top halo
    // enough distance from top to avoid mirroring?
    if (y0 >= kernely->len) {
        // enough distance from left and right to avoid mirroring?
        if (x0 >= kernelx->len && x1 < inarray->n_x - kernelx->len) {
            if (!g_convolve_inner(inarray->ptr + x0 * inarray->stride_x + (y0 - kernely->len) * inarray->stride_y, n_x,
                                  inarray->stride_x, kernely->len, inarray->stride_y, border_top,
                                  n_x * inarray->n_channels, kernelx, FASTFILTERS_BORDER_OPTIMISTIC,
                                  FASTFILTERS_BORDER_OPTIMISTIC, NULL, NULL, 0))
                goto free_and_out;
        } else {
            return false;
        }
    } else {
        return false;
    }

    // bottom halo
    // enough distance from bottom to avoid mirroring?
    if (y1 < inarray->n_y - kernely->len) {
        // enough distance from left and right to avoid mirroring?
        if (x0 >= kernelx->len && x1 < inarray->n_x - kernelx->len) {
            if (!g_convolve_inner(inarray->ptr + x0 * inarray->stride_x + y1 * inarray->stride_y, n_x,
                                  inarray->stride_x, kernely->len, inarray->stride_y, border_bottom,
                                  n_x * inarray->n_channels, kernelx, FASTFILTERS_BORDER_OPTIMISTIC,
                                  FASTFILTERS_BORDER_OPTIMISTIC, NULL, NULL, 0))
                goto free_and_out;
        } else {
            return false;
        }
    } else {
        return false;
    }

    // ROI area x pass
    // enough distance from left and right to avoid mirroring?
    if (x0 >= kernelx->len && x1 < inarray->n_x - kernelx->len) {
        g_convolve_inner(inarray->ptr + x0 * inarray->stride_x + y0 * inarray->stride_y, n_x, inarray->stride_x, n_y,
                         inarray->stride_y, outarray->ptr, outarray->stride_y, kernelx, FASTFILTERS_BORDER_OPTIMISTIC,
                         FASTFILTERS_BORDER_OPTIMISTIC, NULL, NULL, 0);
    } else {
        return false;
    }

    // ROI area y pass
    result = g_convolve_outer(outarray->ptr, n_y, outarray->stride_y, n_x * inarray->n_channels,
                              inarray->stride_x / inarray->n_channels, outarray->ptr, outarray->stride_y, kernely,
                              FASTFILTERS_BORDER_PTR, FASTFILTERS_BORDER_PTR, border_top, border_bottom,
                              n_x * inarray->n_channels);

free_and_out:
    fastfilters_memory_free(border_top);
    fastfilters_memory_free(border_bottom);
    return result;
}