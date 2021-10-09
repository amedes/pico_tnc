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
#include "pico/stdlib.h"

#include "tnc.h"
#include "ax25.h"
#include "send.h"

#define AX25_ADDR_LEN 7

static uint8_t addr[AX25_ADDR_LEN];

#define CON_PID_LEN 2
static const uint8_t con_pid[CON_PID_LEN] = { 0x03, 0xf0 }; // control, PID

void send_unproto(tnc_t *tp, uint8_t *data, int len)
{
    uint8_t byte;
    uint32_t fcs;
    int repeaters = 0;
    int i;

    if (!param.mycall.call[0]) return;      // no mycall
    if (!param.unproto[0].call[0]) return;  // no unproto

    int pkt_len = AX25_ADDR_LEN * 2; // dst + src addr

    // count repeaters
    for (int i = 1; i < UNPROTO_N; i++) {
        if (param.unproto[i].call[0]) { // exist repeater
            pkt_len += AX25_ADDR_LEN;
            repeaters++;
        }
    }

    pkt_len += 2 + len + 2; // CONTL + PID + info + FCS

    if (send_queue_free(tp) < pkt_len + 2) return;

    // packet length
    byte = pkt_len;
    queue_try_add(&tp->send_queue, &byte);
    byte = pkt_len >> 8;
    queue_try_add(&tp->send_queue, &byte);

    // dst addr
    ax25_mkax25addr(addr, &param.unproto[0]);
    addr[6] |= 0x80;                        // set C bit for AX.25 Ver2.2 Command
    for (i = 0; i < AX25_ADDR_LEN; i++) {
        queue_try_add(&tp->send_queue, &addr[i]);
    }
    fcs = ax25_fcs(0, addr, AX25_ADDR_LEN);

    // src addr
    ax25_mkax25addr(addr, &param.mycall);
    if (repeaters == 0) addr[6] |= 1;    // set address extension bit, if no repeaters
    for (i = 0; i < AX25_ADDR_LEN; i++) {
        queue_try_add(&tp->send_queue, &addr[i]);
    }
    fcs = ax25_fcs(fcs, addr, AX25_ADDR_LEN);
    
    // repeaters
    for (int j = 1; j <= repeaters; j++) {
        if (param.unproto[j].call[0]) {
            ax25_mkax25addr(addr, &param.unproto[j]);
            if (j == repeaters) addr[6] |= 1; // set address extension bit, if last repeater
            for (i = 0; i < AX25_ADDR_LEN; i++) {
                queue_try_add(&tp->send_queue, &addr[i]);
            }
            fcs = ax25_fcs(fcs, addr, AX25_ADDR_LEN);
        }
    }

    // control and PID
    for (i = 0; i < CON_PID_LEN; i++) {
        queue_try_add(&tp->send_queue, &con_pid[i]);
    }
    fcs = ax25_fcs(fcs, con_pid, CON_PID_LEN);

    // info
    for (i = 0; i < len; i++) {
        queue_try_add(&tp->send_queue, &data[i]);
    }
    fcs = ax25_fcs(fcs, data, len);

    // fcs
    byte = fcs;
    queue_try_add(&tp->send_queue, &byte);
    byte = fcs >> 8;
    queue_try_add(&tp->send_queue, &byte);
}
