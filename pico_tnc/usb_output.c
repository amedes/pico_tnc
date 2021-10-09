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
#include "class/cdc/cdc_device.h"
#include "pico/sync.h"
#include "pico/util/queue.h"

#define QUEUE_SIZE 1024

static queue_t usb_queue;

void usb_output_init(void)
{
    queue_init(&usb_queue, sizeof(uint8_t), QUEUE_SIZE);
    assert(usb_queue != NULL);
}

void usb_write(uint8_t const *data, int len)
{
    int i;

    if (!queue_is_empty(&usb_queue)) {

        for (i = 0; i < len; i++) {
            if (!queue_try_add(&usb_queue, &data[i])) break;
        }
        return;
    }
    
    int free = tud_cdc_write_available();
        
    if (free >= len) {
        tud_cdc_write(data, len);
        tud_cdc_write_flush();
        return;
    }

    tud_cdc_write(data, free);
    tud_cdc_write_flush();

    for (i = free; i < len; i++) {
        if (!queue_try_add(&usb_queue, &data[i])) break;
    }
}

void usb_write_char(uint8_t ch)
{
    int i = 0;

    if (!queue_is_empty(&usb_queue)) {

        queue_try_add(&usb_queue, &ch);
        return;
    }

    if (tud_cdc_write_available() > 0) {

        tud_cdc_write_char(ch);
        tud_cdc_write_flush();
        return;
    }

    queue_try_add(&usb_queue, &ch);
}

void usb_output(void)
{
    uint8_t data;

    if (queue_is_empty(&usb_queue)) return;

    while (tud_cdc_write_available() > 0) {
        if (queue_try_remove(&usb_queue, &data)) {
            tud_cdc_write_char(data);
        } else {
            break;
        }
    }
    tud_cdc_write_flush();
}

// TinyUSB callback function
void tud_cdc_tx_complete_cb(uint8_t itf)
{
    uint8_t data;

    if (queue_is_empty(&usb_queue)) return; // no queued data

    int free = tud_cdc_write_available();

    while (free > 0) {

        if (!queue_try_remove(&usb_queue, &data)) break;

        tud_cdc_write_char(data);
        --free;
    }
    tud_cdc_write_flush();
}

#if 0
void tud_cdc_rx_wanted_cb(uint8_t itf, char wanted_char)
{
    printf("tud_cdc_rx_wanted_cb(%d), wanted_char = %02x\n", itf, wanted_char);
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    printf("tud_cdc_line_state_cb(%d), dtr = %d, rts = %d\n", itf, dtr, rts);
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p_line_coding)
{
    printf("tud_cdc_line_coding_cb(%d), p_line_coding = %p\n", itf, p_line_coding);
}

void tud_cdc_send_break_cb(uint8_t itf, uint16_t duration_ms)
{
    printf("tud_cdc_send_break_cb(%d), duration_ms = %u\n", itf, duration_ms);
}
#endif
