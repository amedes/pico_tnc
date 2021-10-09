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
#include "send.h"
#include "tty.h"

#define FEND 0xc0
#define FESC 0xdb
#define TFEND 0xdc
#define TFESC 0xdd

#define KISS_PACKET_LEN 1024

enum KISS_COMM {
    KISS_DATA = 0,
    KISS_TXDELAY,
    KISS_P,
    KISS_SLOTTIME,
    KISS_TXTAIL,
    KISS_FULLDUPLEX,
    KISS_SETHARDWARE,
};

void kiss_packet(tty_t *ttyp)
{
    if (ttyp->kiss_idx == 0) return; // packet length == 0

    int type = ttyp->kiss_buf[0]; // kiss type indicator

    if (type == 0xff) {

        // exit kiss mode
        ttyp->kiss_mode = 0;
        return;

    }

    if (ttyp->kiss_idx < 2) return;

    int port = type >> 4;   // port No.
    int comm = type & 0x0f;  // command

    if (port >= PORT_N) return;

    tnc_t *tp = &tnc[port];
    int val = ttyp->kiss_buf[1];

    // kiss command
    switch (comm) {

        case KISS_DATA:
            // send kiss packet
            send_packet(tp, &ttyp->kiss_buf[1], ttyp->kiss_idx - 1); // delete kiss type byte
            break;

        case KISS_TXDELAY:
            tp->kiss_txdelay = val;
            break;

        case KISS_P:
            tp->kiss_p = val;
            break;

        case KISS_SLOTTIME:
            tp->kiss_slottime = val;
            break;

        case KISS_FULLDUPLEX:
            tp->kiss_fullduplex = val;
            break;
    }
}

void kiss_input(tty_t * ttyp, int ch)
{
    switch (ttyp->kiss_state) {

        case KISS_OUTSIDE:
            if (ch == FEND) {
                ttyp->kiss_idx = 0;
                ttyp->kiss_timeout = tnc_time();
                ttyp->kiss_state = KISS_INSIDE;
            }
            break;

        case KISS_INSIDE:

            switch (ch) {
                case FEND:
                    kiss_packet(ttyp);      // send kiss packet
                    ttyp->kiss_state = KISS_OUTSIDE;
                    break;

                case FESC:
                    ttyp->kiss_state = KISS_FESC;
                    break;

                default:
                    if (ttyp->kiss_idx >= KISS_PACKET_LEN) {
                        ttyp->kiss_state = KISS_ERROR;
                        break;
                    }
                    ttyp->kiss_buf[ttyp->kiss_idx++] = ch;
            }
            break;

        case KISS_FESC:

            switch (ch) {
                case TFEND:
                    ch = FEND;
                    break;
                    
                case TFESC:
                    ch = FESC;
                    break;
            }

            if (ttyp->kiss_idx >= KISS_PACKET_LEN) {
                ttyp->kiss_state = KISS_ERROR;
                break;
            }

            ttyp->kiss_buf[ttyp->kiss_idx++] = ch;
            ttyp->kiss_state = KISS_INSIDE;
            break;

        case KISS_ERROR:
            // discard chars until FEND
            if (ch == FEND) ttyp->kiss_state = KISS_OUTSIDE;
    }
}

void kiss_output(tty_t *ttyp, tnc_t *tp)
{
    int len = tp->data_cnt;
    uint8_t *data = tp->data;

    // KISS start
    tty_write_char(ttyp, FEND);

    // kiss type, port, data frame 0
    uint8_t type = tp->port << 4;
    tty_write_char(ttyp, type);

    for (int i = 0; i < len - 2; i++) { // delete FCS

        int ch =  data[i];

        switch (ch) {
            case FEND:
                tty_write_char(ttyp, FESC);
                tty_write_char(ttyp, TFEND);
                break;

            case FESC:
                tty_write_char(ttyp, FESC);
                tty_write_char(ttyp, TFESC);
                break;

            default:
                tty_write_char(ttyp, ch);
        }
    }

    // KISS end
    tty_write_char(ttyp, FEND);
}
