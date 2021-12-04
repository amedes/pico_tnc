/*
Copyright (c) 2021, JN1DFF
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

#include "tnc.h"
#include "ax25.h"
#include "flash.h"

uint32_t __tnc_time;

tnc_t tnc[PORT_N];

param_t param = {
    .mycall = { 0, 0, },
    .unproto = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, },
    .myalias = { 0, 0 },
    .btext = "",
    .txdelay = 100,
    .echo = 1,
    .gps = 0,
    .trace = 0,
    .mon = 0,
    .digi = 0,
    .beacon = 0,
};

void tnc_init(void)
{
    // filter initialization
    // LPF
    static const filter_param_t flt_lpf = {
        .size = FIR_LPF_N,
        .sampling_freq = SAMPLING_RATE,
        .pass_freq = 0,
        .cutoff_freq = 1200,
    };
    int16_t *lpf_an, *bpf_an;

    lpf_an = filter_coeff(&flt_lpf);

#if 0
    printf("LPF coeffient\n");
    for (int i = 0; i < flt_lpf.size; i++) {
        printf("%d\n", lpf_an[i]);
    }
#endif
    // BPF
    static const filter_param_t flt_bpf = {
        .size = FIR_BPF_N,
        .sampling_freq = SAMPLING_RATE,
        .pass_freq = 900,
        .cutoff_freq = 2500,
    };
    bpf_an = filter_coeff(&flt_bpf);
#if 0
    printf("BPF coeffient\n");
    for (int i = 0; i < flt_bpf.size; i++) {
        printf("%d\n", bpf_an[i]);
    }
#endif
    // PORT initialization
    for (int i = 0; i < PORT_N; i++) {
        tnc_t *tp = &tnc[i];

        // receive
        tp->port = i;
        tp->state = FLAG;
        filter_init(&tp->lpf, lpf_an, FIR_LPF_N);
        filter_init(&tp->bpf, bpf_an, FIR_BPF_N);

        // send queue
        queue_init(&tp->send_queue, sizeof(uint8_t), SEND_QUEUE_LEN);
        tp->send_state = SP_IDLE;

        tp->cdt = 0;
        tp->kiss_txdelay = 50;
        tp->kiss_p = 63;
        tp->kiss_slottime = 10;
        tp->kiss_fullduplex = 0;

        // calibrate
        tp->do_nrzi = true;
    }

    //printf("%d ports support\n", PORT_N);
    //printf("DELAYED_N = %d\n", DELAYED_N);

    // read flash
    flash_read(&param, sizeof(param));

    // set kiss txdelay
    if (param.txdelay > 0) {
        tnc[0].kiss_txdelay = param.txdelay * 2 / 3;
    }
}
