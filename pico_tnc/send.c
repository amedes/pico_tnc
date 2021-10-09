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
 * send.c - send packet
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/structs/uart.h"
#include "pico/util/queue.h"

#include "tnc.h"
#include "send.h"
#include "ax25.h"

//#include "wave_table.h"
#include "wave_table_132mhz.h"

static const int pwm_pins[] = {
    14, // port 0
    12,
    10,
    8,
    6,
    4,
    2,
    0
};

static const int ptt_pins[] = {
    //PICO_DEFAULT_LED_PIN
    15, // port 0
    13,
    11,
    9,
    7,
    5,
    3,
    1
};

#define LED_PIN PICO_DEFAULT_LED_PIN

#define ISR_PIN 15

static void __isr dma_handler(void)
{
    int int_status = dma_hw->ints0;

    //printf("irq: %08x\n", dma_hw->ints0);


    for (int i = 0; i < PORT_N; i++) {
        tnc_t *tp = &tnc[i];

    if (int_status & tp->data_chan_mask) {

        // generate modem signal
        uint32_t *addr;
        if (queue_try_remove(&tp->dac_queue, &addr)) {
            
            dma_channel_set_read_addr(tp->ctrl_chan, addr, true);
#if 0
            printf("dma_hander: block = %p\n", &block[0]);
            for (int i = 0; i < CONTROL_N + 1; i++) {
                printf("block[%d] = %p\n", i, block[i]);
            }
#endif
        } else {
            gpio_put(tp->ptt_pin, 0); // PTT off
            pwm_set_chan_level(tp->pwm_slice, PWM_CHAN_A, 0); // set pwm level 0
            tp->busy = false;
            //printf("(%u) dma_handler: queue is empty, port = %d, data_chan = %d, ints = %08x\n", tnc_time(), tp->port, tp->data_chan, int_status);
        }
        //dma_hw->ints0 = tp->data_chan_mask;
    }
    } // for

    dma_hw->ints0 = int_status;

}

static void send_start(tnc_t *tp)
{
    if (!tp->busy) {
        gpio_put(tp->ptt_pin, 1); // PTT on
        tp->busy = true;
        //printf("restart dma, ctrl = %08x, port = %d\n", dma_hw->ch[tp->data_chan].ctrl_trig, tp->port);
        dma_channel_set_read_addr(tp->data_chan, NULL, true);
    }
}

bool send_packet(tnc_t *tp, uint8_t *data, int len)
{
    int length = len + 2; // fcs 2 byte
    uint8_t byte;

    if (send_queue_free(tp) < length + 2) return false; // queue has no room

    // packet length
    byte = length;
    queue_try_add(&tp->send_queue, &byte);
    byte = length >> 8;
    queue_try_add(&tp->send_queue, &byte);

    // send packet to queue
    for (int i = 0; i < len; i++) {
        queue_try_add(&tp->send_queue, &data[i]);
    }

    int fcs = ax25_fcs(0, data, len);

    // fcs
    byte = fcs;
    queue_try_add(&tp->send_queue, &byte);
    byte = fcs >> 8;
    queue_try_add(&tp->send_queue, &byte);

    return true;
}

int send_byte(tnc_t *tp, uint8_t data, bool bit_stuff)
{
    int idx = 0;

    // generate modem signal
    if (!queue_is_full(&tp->dac_queue)) {
        //uint8_t data = rand();
            
        int byte = data | 0x100; // sentinel

        int bit = byte & 1;
        while (byte > 1) { // check sentinel

            if (!bit) tp->level ^= 1; // NRZI, invert if original bit == 0

            // make Bell202 CPAFSK audio samples
            tp->dma_blocks[tp->next][idx++] = phase_tab[tp->level][tp->phase]; // 1: mark, 0: space

            if (!tp->level) { // need adjust phase if space (2200Hz)
                if (--tp->phase < 0) tp->phase = PHASE_CYCLE - 1;
            }

            // bit stuffing
            if (bit_stuff) {

                if (bit) {

                    if (++tp->cnt_one >= BIT_STUFF_BITS) {
                        // insert "0" bit
                        bit = 0;
                        continue; // while
                    }

                } else {

                    tp->cnt_one = 0;
                }

            }

            byte >>= 1;
            bit = byte & 1;
        }
        
        // insert DMA end mark
        tp->dma_blocks[tp->next][idx] = NULL;

        // send fsk data to dac queue
        uint16_t const **block = &tp->dma_blocks[tp->next][0];
        if (queue_try_add(&tp->dac_queue, &block)) {
#if 0
            if (!tp->busy) {
                tp->busy = true;
                dma_channel_set_read_addr(tp->data_chan, NULL, true);
                //printf("restart dma, ctrl = %08x, port = %d\n", dma_hw->ch[tp->data_chan].ctrl_trig, tp->port);
            }
#endif
            if (++tp->next >= DAC_BLOCK_LEN) tp->next = 0;
                
        
            //printf("main: queue add success\n");
        } else {
            printf("main: queue add fail\n");
        }

    } else {

        send_start(tp);

    }

    return idx;
}


void send_init(void)
{
    // set system clock, PWM uses system clock
    set_sys_clock_khz(SYS_CLK_KHZ, true);

    // initialize tnc[]
    for (int i = 0; i < PORT_N; i++) {
        tnc_t *tp = &tnc[i];

        // port No.
        tp->port = i;

        // queue
        queue_init(&tp->dac_queue, sizeof(uint32_t *), DAC_QUEUE_LEN);

        // PTT pins
        tp->ptt_pin = ptt_pins[i];
        gpio_init(tp->ptt_pin);
        gpio_set_dir(tp->ptt_pin, true); // output
        gpio_put(tp->ptt_pin, 0);

        // PWM pins
        tp->pwm_pin = pwm_pins[i];
        // PWM configuration
        gpio_set_function(tp->pwm_pin, GPIO_FUNC_PWM);
        // PWM slice
        tp->pwm_slice = pwm_gpio_to_slice_num(tp->pwm_pin);

        // PWM configuration
        pwm_config pc = pwm_get_default_config();
        pwm_config_set_clkdiv_int(&pc, 1); // 1.0
        pwm_config_set_wrap(&pc, PWM_CYCLE - 1);
        pwm_init(tp->pwm_slice, &pc, true);   // start PWM

        // DMA
        tp->ctrl_chan = dma_claim_unused_channel(true);
        tp->data_chan = dma_claim_unused_channel(true);
        tp->data_chan_mask = 1 << tp->data_chan;

        //printf("port %d: pwm_pin = %d, ptt_pin = %d, ctrl_chan = %d, data_chan = %d\n", tp->port, tp->pwm_pin, tp->ptt_pin, tp->ctrl_chan, tp->data_chan);
        
        // DMA control channel
        dma_channel_config dc = dma_channel_get_default_config(tp->ctrl_chan);
        channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
        channel_config_set_read_increment(&dc, true);
        channel_config_set_write_increment(&dc, false);

        dma_channel_configure(
            tp->ctrl_chan,
            &dc,
            &dma_hw->ch[tp->data_chan].al3_read_addr_trig, // Initial write address
            NULL,                        // Initial read address
            1,                                         // Halt after each control block
            false                                      // Don't start yet
        );

        // DMA data channel
        dc = dma_channel_get_default_config(tp->data_chan);
        channel_config_set_transfer_data_size(&dc, DMA_SIZE_16); // PWM level is 16 bits
        channel_config_set_dreq(&dc, DREQ_PWM_WRAP0 + tp->pwm_slice);
        channel_config_set_chain_to(&dc, tp->ctrl_chan);
        channel_config_set_irq_quiet(&dc, true);
        // set high priority bit
        //dc.ctrl |= DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS;

        dma_channel_configure(
            tp->data_chan,
            &dc,
            &pwm_hw->slice[tp->pwm_slice].cc,
            NULL,           // Initial read address and transfer count are unimportant;
            BIT_CYCLE,      // audio data of 1/1200 s
            false           // Don't start yet.
        );

        // configure IRQ
        dma_channel_set_irq0_enabled(tp->data_chan, true);
    }

    // configure IRQ
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    //irq_add_shared_handler(DMA_IRQ_0, dma_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(DMA_IRQ_0, true);

    // ISR time measurement
    gpio_init(ISR_PIN);
    gpio_set_dir(ISR_PIN, true);

#define SMPS_PIN 23

    // SMPS set PWM mode
    gpio_init(SMPS_PIN);
    gpio_set_dir(SMPS_PIN, true);
    gpio_put(SMPS_PIN, 1);
}

enum SEND_STATE {
    SP_IDLE = 0,
    SP_WAIT_CLR_CH,
    SP_P_PERSISTENCE,
    SP_WAIT_SLOTTIME,
    SP_PTT_ON,
    SP_SEND_FLAGS,
    SP_DATA_START,
    SP_DATA,
    SP_ERROR,
};

int send_queue_free(tnc_t *tp)
{
    return SEND_QUEUE_LEN - queue_get_level(&tp->send_queue);
}

void send(void)
{
    uint8_t data;

    tnc_t *tp = &tnc[0];
    while (tp < &tnc[PORT_N]) {

        switch (tp->send_state) {
            case SP_IDLE:
                //printf("(%d) send: SP_IDEL\n", tnc_time());
                if (!queue_is_empty(&tp->send_queue)) {
                    tp->send_state = SP_WAIT_CLR_CH;
                    continue;
                }
                break;
                    
            case SP_WAIT_CLR_CH:
                //printf("(%d) send: SP_WAIT_CLR_CH\n", tnc_time());
                if (tp->kiss_fullduplex || !tp->cdt) {
                    tp->send_state = SP_P_PERSISTENCE;
                    continue;
                }
                break;

            case SP_P_PERSISTENCE:
                data = rand();
                //printf("(%d) send: SP_P_PERSISTENCE, rnd = %d\n", tnc_time(), data);
                if (data <= tp->kiss_p) {
                    tp->send_state = SP_PTT_ON;
                    continue;
                }
                tp->send_time = tnc_time();
                tp->send_state = SP_WAIT_SLOTTIME;
                break;

            case SP_WAIT_SLOTTIME:
                //printf("(%d) send: SP_WAIT_SLOTTIME\n", tnc_time());
                if (tnc_time() - tp->send_time >= tp->kiss_slottime) {
                    tp->send_state = SP_WAIT_CLR_CH;
                    continue;
                }
                break;

            case SP_PTT_ON:
                //printf("(%d) send: SP_PTT_ON\n", tnc_time());
                //gpio_put(tp->ptt_pin, 1);
                tp->send_len = (tp->kiss_txdelay * 3) / 2 + 1; // TXDELAY * 10 [ms] into number of flags
                tp->send_state = SP_SEND_FLAGS;
                /* FALLTHROUGH */

            case SP_SEND_FLAGS:
                //printf("(%d) send: SP_SEND_FLAGS\n", tnc_time());
                while (tp->send_len > 0 && send_byte(tp, AX25_FLAG, false)) { // false: bit stuffing off
                    --tp->send_len;
                }
                if (tp->send_len > 0) break;
                tp->cnt_one = 0;                // bit stuffing counter clear
                tp->send_state = SP_DATA_START;
                /* FALLTHROUGH */

            case SP_DATA_START:
                if (!queue_try_remove(&tp->send_queue, &data)) {
                    tp->send_state = SP_IDLE;
                    break;
                }
                // read packet length low byte
                tp->send_len = data;
                if (!queue_try_remove(&tp->send_queue, &data)) {
                    printf("send: send_queue underrun, len\n");
                    tp->send_state = SP_IDLE;
                    break;
                }
                // read packet length high byte
                tp->send_len += data << 8;
                //printf("(%d) send: SP_DATA_START, len = %d\n", tnc_time(), tp->send_len);
                if (!queue_try_remove(&tp->send_queue, &data)) {
                    printf("send: send_queue underrun, data(1)\n");
                    tp->send_state = SP_IDLE;
                    break;
                }
                tp->send_data = data;
                --tp->send_len;
                tp->send_state = SP_DATA;
                /* FALLTHROUGH */

            case SP_DATA:
                //printf("(%d) send: SP_DATA\n", tnc_time());
                if (!send_byte(tp, tp->send_data, true)) break;
                if (tp->send_len <= 0) {
                    tp->send_len = 1;
                    tp->send_state = SP_SEND_FLAGS;
                    send_start(tp);
                    continue;
                }
                if (!queue_try_remove(&tp->send_queue, &data)) {
                    printf("send: send_queue underrun, data(2)\n");
                    tp->send_state = SP_IDLE;
                    break;
                }
                --tp->send_len;
                tp->send_data = data;
                continue;

            case SP_ERROR:
                //printf("(%d) send: SP_ERROR\n", tnc_time());
                while (queue_try_remove(&tp->send_queue, &data)) {
                }
                tp->send_state = SP_IDLE;
        }

        tp++;
    } // while
}
