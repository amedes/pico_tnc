/*
Copyright (c) 2021, Kazuhisa Yokota, JN1DFF
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, 
  this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, 
  this list of conditions and the following disclaimer in the documentation 
  and/or other materials provided with the distribution.
* Neither the name of the <organization> nor the names of its contributors 
  may be used to endorse or promote products derived from this software 
  without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * fir_filter.c
 *
 * FIR filter routine
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
//#include <xtensa/config/core.h>
//#include <esp_log.h>

#include "filter.h"
#include "tnc.h"

#define TAG "filter"

//#define OVERFLOW_CHECK 1

int filter(filter_t *fp, int value)
{
    int32_t sum = 0;
    int i;
    int an_idx;
#ifdef OVERFLOW_CHECK
    int64_t suml = 0;
#endif

    //if (x_idx == 0) printf("%d ", adc);
    //
    fp->x[fp->index] = value;

    // index of circular buffer
    an_idx = fp->size - 1 - fp->index;

    if (++fp->index >= fp->size) fp->index = 0;

    for (i = 0; i < fp->size; i++) {
        int v = fp->an[an_idx + i] * fp->x[i];
	    sum += v;
#ifdef OVERFLOW_CHECK
	    suml += v;
#endif
    }

#if 0
    static int max = 0;
    static int min = (1LL << 31) - 1;
    if (sum > max) {
        max = sum;
        printf("max = %d\n", max);
    } else if (sum < min) {
        min = sum;
        printf("min = %d\n", min);
    }
#endif

#ifdef OVERFLOW_CHECK
    if (sum != suml) {
      printf("overflow: sum = %d, suml = %lld\n", sum, suml);
      abort();
    }
#endif

    //if (--fp->an_idx < 0) fp->an_idx = fp->size - 1;

    //printf("value = %d, sum = %d, sum >> 16 = %d\n", value, sum, sum >> 16);

    return sum;
}

// sinc function
static float sinc(float x)
{
    if (fabsf(x) < 1e-6) return 1.0;

    return sinf(x) / x;
}

// window function
static float windowf(float x)
{
    return 0.54 + 0.46 * cosf(x);
}

// low pass filter initialization
int16_t *filter_coeff(filter_param_t const *f)
{
    // calculate FIR coefficient
    const int fp = f->pass_freq;	// pass frequency
    const int fc = f->cutoff_freq;	// cut off frequency
    const int fck = f->sampling_freq;	// sampling frequency
    const int size = f->size;
    //const float Tck = 1.0 / fck;
    const float Rp = (float)fp / (float)fck;
    const float Rc = (float)fc / (float)fck;
    //const int M = 7;
    const int M = (size - 1) / 2;
    const int A = 1 << 15; // amplitude
    int16_t *an;
    int n;

    //fprintf(stderr, "size = %d, sampling freq = %d, pass freq = %d, cutoff freq = %d\n",
    //    f->size, f->sampling_freq, f->pass_freq, f->cutoff_freq);

    an = calloc(size * 2 - 1, sizeof(int16_t));

    if (an == NULL) {
        perror("calloc fail");
	    exit(1);
    }

    // finite impulse response
    for (n = -M; n <= M; n++) {
        int val;

	    val = A * 2 * (Rc * sinc(2 * M_PI * Rc * n) - Rp * sinc(2 * M_PI * Rp * n)) * windowf(M_PI * n / M) + 0.5;

	    an[n + M] = val;
	    //an[n + M] = 2 * R * A * sinc(2 * M_PI * R * n);
	    //fprintf(stderr, "an[%d] = %d\n", n, an[n + M]);
    }

    // prepare circular buffer
    for (n = 0; n < size - 1; n++) {
        an[size + n] = an[n];
    }

    return an;
}

void filter_init(filter_t *fp, int16_t an[], int size)
{
    int16_t *p;

    fp->an = an;
    fp->size = size;

    p = calloc((size + 3) & ~3, sizeof(int16_t));
    if (p == NULL) {
        perror("calloc");
	    exit(1);
    }
    fp->x = p;
}
