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

#include "tnc.h"
#include "packet_table.h"
#include "ax25.h"

enum TEST_STATE {
    TEST_IDLE = 0,
    TEST_PACKET_START,
    TEST_WAIT,
    TEST_ERROR,
};

#if 0
static int test_state = TEST_IDLE;
static const uint8_t *ptp = packet_table;
static const uint8_t *packet;
static uint16_t packet_len;
static int wait_time;
#endif

#define SEND_INTERVAL (30 * 100)     // interval 10 ms unit
#define SLEEP (10 * 100)            // wait time when buffer full
#define MAX_SLEEP (~0)              // int max

static int port_mask = 0;

void test_init(int mask)
{
    port_mask = mask & ((1 << PORT_N) - 1);

    for (int port = 0; port < PORT_N; port++) {
        
        if (!(port_mask & (1 << port))) continue;

        tnc_t *tp = &tnc[port];

        tp->test_state = TEST_IDLE;
        tp->ptp = packet_table;
    }
}

void test(void)
{
    uint8_t data;
    uint16_t fcs;

    //if (port < 0 || port >= PORT_N) return;

    for (int port = 0; port < PORT_N; port++) {

        if (!(port_mask & (1 << port))) continue;

        tnc_t *tp = &tnc[port];

        switch (tp->test_state) {
        case TEST_IDLE:
            //printf("(%d) test: TEST_IDLE\n", tnc_time());
            tp->packet_len = *tp->ptp++;
            if (tp->packet_len == 0) {
#if 0
                printf("test: send all packets, stop\n");
                wait_time = MAX_SLEEP;
                test_state = TEST_WAIT;
                break;
#else
                printf("(%u) test: send all test packets done, port = %d\n", tnc_time(), tp->port);
                tp->ptp = packet_table;
                tp->packet_len = *tp->ptp++;
#endif
            }
            //printf("(%d) test: test packet\n", tnc_time());
            tp->packet = tp->ptp;
            tp->ptp += tp->packet_len;
            tp->test_state = TEST_PACKET_START;
            /* FALLTHROUGH */

        case TEST_PACKET_START:
            //printf("(%d) test: TEST_PACKET_START\n", tnc_time());
            if (SEND_QUEUE_LEN - queue_get_level(&tp->send_queue) < tp->packet_len + 2) { // "2" means length field (16bit)
                //printf("(%u) test: send_queue is full, port = %d\n", tnc_time(), tp->port);
                break;
            }
            // packet length
            for (int i = 0; i < 2; i++) {
                data = tp->packet_len >> (i * 8);
                if (!queue_try_add(&tp->send_queue, &data)) {
                    tp->test_state = TEST_ERROR;
                    break;
                }
            }

#define CALC_FCS 1

            // packet data
#ifdef CALC_FCS
            for (int i = 0; i < tp->packet_len - 2; i++) {
#else
            for (int i = 0; i < tp->packet_len; i++) {
#endif
                if (!queue_try_add(&tp->send_queue, &tp->packet[i])) {
                    tp->test_state = TEST_ERROR;
                    break;
                }
            }
#ifdef CALC_FCS
            // fcs
            fcs = ax25_fcs(0, tp->packet, tp->packet_len - 2);
            // add fcs
            for (int i = 0; i < 2; i++) {
                data = fcs >> (i * 8);
                if (!queue_try_add(&tp->send_queue, &data)) {
                    tp->test_state = TEST_ERROR;
                    break;
                }
            }
#endif
            tp->wait_time = tnc_time();
            tp->test_state = TEST_WAIT;
            break;

        case TEST_WAIT:
            //printf("(%d) test: TEST_WAIT\n", tnc_time());
            if (tnc_time() - tp->wait_time < SEND_INTERVAL) break;
            tp->test_state = TEST_IDLE;
            break;

        case TEST_ERROR:
            printf("(%d) test: TEST_ERROR\n", tnc_time());
            printf("test: error occured, stop\n");
            tp->wait_time = MAX_SLEEP;
            tp->test_state = TEST_WAIT;
        }
    }
}
