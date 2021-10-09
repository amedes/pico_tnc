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

#include <stdio.h>
#include <stdint.h>
#include <math.h>

//#include "timer.h"
#include "filter.h"
#include "tnc.h"

int bell202_decode(tnc_t *tp, int adc)
{
    int m;
    int sum;
    int val;

    //fprintf(stderr,"adc = %d\n");

    //val = input_bpf(adc);
    // Bandpass filter
    val = filter(&tp->bpf, adc) >> ADC_BIT;
    //val = adc;

    //printf("val = %d\n", val);

    m = val * tp->delayed[tp->delay_idx];

    //printf("val = %d, m = %d\n", val, m);
    //printf("%d\n", m);

    tp->delayed[tp->delay_idx] = val;
    if (++tp->delay_idx >= DELAYED_N) tp->delay_idx = 0;

#if 0
    x[x_idx] = m >> 12;
    sum = 0;
    for (i = 0; i < FIR_LPF_N; i++) {
	sum += an[i] * x[(x_idx + i) % FIR_LPF_N];
#if 0
	if (sum > (1 << 30)) printf("%d,", sum);
	else
	if (sum < -(1 << 30)) printf("%d,", sum);
#endif
    }
    x_idx += FIR_LPF_N - 1;
    x_idx %= FIR_LPF_N;

    //printf("%d, %d, %d\n", adc, m >> 8, sum >> 8);
#else

    sum = filter(&tp->lpf, m >> 16);

    //printf("%d, %d, %d, %d\n", adc, val, m, sum);
#endif

#ifdef LPF_FLOAT
    return sum * 65536;
#else
    return sum;
#endif
}

#ifdef BELL202_SYNC

#define LOW_N SAMPLING_N
#define HIGH_N (LOW_N * 6 / 11)

static int16_t low_i_tab[LOW_N];
static int16_t low_q_tab[LOW_N];
static int16_t high_i_tab[HIGH_N];
static int16_t high_q_tab[HIGH_N];

static const int16_t low_tab[LOW_N] = {
    -15704,
    2338,
    19637,
    30701,
    32018,
    23170,
    6965,
    -11451,
    -26231,
    -32684,
    -28759,
};

static const int16_t high_tab[HIGH_N + 1] = {
    23170,
    -8481,
    -31650,
    -23170,
    8481,
    31650,
    23170,
};

// Synch decode
int bell202_decode2(tnc_t *tp, int adc)
{
    int m;
    int sum;
    int val;
    int i;
    values_t *vp;

    // Band pass filter
#if ADC_BIT == 8
    //val = filter(&tp->bpf, adc) >> 8;
    val = filter(&tp->bpf, adc) >> 12;
#else
    val = filter(&tp->bpf, adc) >> 12;
#endif

    //printf("val = %d\n", val);

    vp = &tp->values[tp->values_idx];

    // subtract old values
    tp->sum_low_i -= vp->low_i;
    tp->sum_low_q -= vp->low_q;
    tp->sum_high_i -= vp->high_i;
    tp->sum_high_q -= vp->high_q;

    // add new values
#if 0
    tp->sum_low_i += (vp->low_i = val * low_i_tab[tp->low_idx]);
    tp->sum_low_q += (vp->low_q = val * low_q_tab[tp->low_idx]);
    tp->sum_high_i += (vp->high_i = val * high_i_tab[tp->high_idx]);
    tp->sum_high_q += (vp->high_q = val * high_q_tab[tp->high_idx]);
#endif
    tp->sum_low_i += (vp->low_i = val * low_tab[tp->low_idx]);
    tp->sum_low_q += (vp->low_q = val * low_tab[LOW_N - 1 - tp->low_idx]);
    tp->sum_high_i += (vp->high_i = val * high_tab[tp->high_idx]);
    tp->sum_high_q += (vp->high_q = val * high_tab[HIGH_N - tp->high_idx]);

    int sqr_li = tp->sum_low_i >> 16;
    int sqr_lq = tp->sum_low_q >> 16;
    int sqr_hi = tp->sum_high_i >> 16;
    int sqr_hq = tp->sum_high_q >> 16;

    // compare
    m = (sqr_li * sqr_li + sqr_lq * sqr_lq)
      - (sqr_hi * sqr_hi + sqr_hq * sqr_hq);

    // low pass filter
    sum = filter(&tp->lpf, m >> 16);

    //printf("%d, %d, %d, %d\n", adc, val, m, sum);

    // advance indexes
    if (++tp->values_idx >= SAMPLING_N) tp->values_idx = 0;
    if (++tp->low_idx >= LOW_N) tp->low_idx = 0;
    if (++tp->high_idx >= HIGH_N) tp->high_idx = 0;

    return sum;
}

#endif // BELL202_SYNC

void bell202_init(void)
{
#ifdef BELL202_SYNC
    // initialize sin, cos table
    int i;

#define AMP 32767

    //printf("bell202: I/Q tables\n");
    
    for (i = 0; i < LOW_N; i++) {
        float t = M_PI * 2 * i / LOW_N + M_PI / 4;

        low_i_tab[i] = cosf(t) * AMP + 0.5;
        low_q_tab[i] = sinf(t) * AMP + 0.5;

        printf("%d, %d, ", low_i_tab[i], low_q_tab[i]);
    
        if (i < HIGH_N) {
            t = M_PI * 2 * i / HIGH_N + M_PI / 4;

            high_i_tab[i] = cosf(t) * AMP + 0.5;
            high_q_tab[i] = sinf(t) * AMP + 0.5;

            //printf("%d, %d, ", high_i_tab[i], high_q_tab[i]);
        }
        //printf("\n");
    }
#endif
}
