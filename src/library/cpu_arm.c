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
#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

static bool _supports_avx2()
{
    return true;
}

static bool _supports_avx()
{
    return true;
}

static bool _supports_fma()
{
    return true;
}


static bool g_supports_avx = false;
static bool g_supports_fma = false;
static bool g_supports_avx2 = false;

void fastfilters_cpu_init(void)
{
    g_supports_avx = _supports_avx();
    g_supports_fma = _supports_fma();
    g_supports_avx2 = _supports_avx2();
}

bool DLL_PUBLIC fastfilters_cpu_enable(fastfilters_cpu_feature_t feature, bool enable)
{
    switch (feature) {
    case FASTFILTERS_CPU_AVX:
        if (enable)
            g_supports_avx = _supports_avx();
        else
            g_supports_avx = false;
        break;
    case FASTFILTERS_CPU_FMA:
        if (enable)
            g_supports_fma = _supports_fma();
        else
            g_supports_fma = false;
        break;
    case FASTFILTERS_CPU_AVX2:
        if (enable)
            g_supports_avx2 = _supports_avx2();
        else
            g_supports_avx2 = false;
        break;
    default:
        return false;
    }

    return fastfilters_cpu_check(feature);
}

bool DLL_PUBLIC fastfilters_cpu_check(fastfilters_cpu_feature_t feature)
{
    switch (feature) {
    case FASTFILTERS_CPU_AVX:
        return g_supports_avx;
    case FASTFILTERS_CPU_FMA:
        return g_supports_fma;
    case FASTFILTERS_CPU_AVX2:
        return g_supports_avx2;
    default:
        return false;
    }
}