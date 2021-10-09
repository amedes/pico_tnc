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
#include <string.h>
#include "pico/stdlib.h"

#include "tnc.h"
#include "ax25.h"
#include "send.h"

#define SSID_LOC 6
#define H_BIT 0x80
#define AX25_MIN_LEN (7 + 7 + 1 + 1)    // dst, src, control, PID
#define MAX_DIGIPEATER 8

static const uint8_t con_pid_ui[2] = { 0x03, 0xf0, };

void digipeat(tnc_t *tp)
{
    uint8_t *packet = tp->data;
    int len = tp->data_cnt;

    if (len < AX25_MIN_LEN) return; // too short

    if (!ax25_ui(packet, len)) return; // not UI packet

    int offset = AX25_ADDR_LEN; // src addr

    if (packet[offset + SSID_LOC] & 1) return; // no repeaters

    offset += AX25_ADDR_LEN; // 1st digipeater addr

    int digis = 1;

    while (offset + AX25_ADDR_LEN <= len) {

        if (!(packet[offset + SSID_LOC] & H_BIT)) { // has not been digipeated yet

            if (ax25_callcmp(&param.mycall, &packet[offset])
                || ax25_callcmp(&param.myalias, &packet[offset])) { // addr matched

                packet[offset + SSID_LOC] |= H_BIT;  // set H bit
                send_packet(tp, packet, len - 2);    // delete FCS
                packet[offset + SSID_LOC] &= ~H_BIT; // clear H bit
            }

            break;
        }

        if (++digis >= MAX_DIGIPEATER) return;

        offset += AX25_ADDR_LEN; // next digipeater addr
    }
}
