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
// For ADC input:
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "pico/sem.h"
#include "hardware/watchdog.h"

// For resistor DAC output:
//#include "pico/multicore.h"
//#include "hardware/pio.h"
//#include "resistor_dac.pio.h"

#include "tnc.h"
#include "decode.h"
#include "bell202.h"
//#include "timer.h"

// Channel 0 is GPIO26
#define ADC_GPIO 26

#define BUF_NUM 16
#define BUF_LEN ((BAUD_RATE * SAMPLING_N * PORT_N + 50) / 100) // ADC samples in 10 ms

#define DEBUG_PIN 22

#if ADC_BIT == 8
static uint8_t buf[BUF_NUM][BUF_LEN];
#else
static uint16_t buf[BUF_NUM][BUF_LEN];
#endif

// DMA channel for ADC
static int dma_chan;

static semaphore_t sem;

static void dma_handler(void) {
    static int buf_next = 1;
#if 0
    if (sem_available(&sem) == BUF_NUM) {
        printf("ADC: DMA buffer overrun\n");
        assert(false);
    }
#endif

    // set buffer address
    dma_channel_set_write_addr(dma_chan, buf[buf_next], true); // trigger DMA

    // release semaphore
    sem_release(&sem);

    // advance ADC buffer
    ++buf_next;
    buf_next &= BUF_NUM - 1;

    // clear the interrupt request, ADC DMA using irq1
    dma_hw->ints1 = dma_hw->ints1;
}

static const uint8_t cdt_pins[] = {
#ifdef PICO_DEFAULT_LED_PIN
    PICO_DEFAULT_LED_PIN,
#else
    20, // port 0
#endif
    21, // port 1
    22, // port 2
    22, // dummy
    22, // dummy
};


void receive_init(void)
{
    // Init GPIO for analogue use: hi-Z, no pulls, disable digital input buffer.
    uint8_t adc_rr_mask = 0;    
    for (int i = 0; i < PORT_N; i++) {

        // initialize GPIO pin for ADC
        int adc_pin = ADC_GPIO + i;

        adc_gpio_init(adc_pin);
        adc_rr_mask |= 1 << i;

        tnc_t *tp = &tnc[i];

        // set cdt led pin
        uint8_t pin = cdt_pins[i];

        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        tp->cdt_pin = pin;

        // initialize variables
        tp->cdt = false;
        tp->cdt_lvl = 0;
        tp->avg = 0;
    }

    adc_init();
    adc_select_input(0); // start at ADC 0
    adc_set_round_robin(adc_rr_mask);
    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // We won't see the ERR bit because of 8 bit reads; disable.
#if ADC_BIT == 8
        true     // Shift each sample to 8 bits when pushing to FIFO
#else
        false    // ADC sample 12 bits
#endif
    );

#define ADC_CLK (48ULL * 1000 * 1000)

    adc_hw->div = (ADC_CLK * 256 + ADC_SAMPLING_RATE/2) / ADC_SAMPLING_RATE - 256; // (INT part - 1) << 8 | FRAC part
//
    //printf("adc.div = 0x%x, freq. = %f Hz\n", adc_hw->div, 48e6 * 256 / (adc_hw->div + 256));

    //sleep_ms(1000);

    // Set up the DMA to start transferring data as soon as it appears in FIFO
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

    //printf("initialize DMA channel: %d\n", dma_chan);

    // Reading from constant address, writing to incrementing byte addresses
#if ADC_BIT == 8
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
#else
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
#endif
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);

    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&cfg, DREQ_ADC);
    channel_config_set_enable(&cfg, true);
    
    dma_channel_configure(dma_chan, &cfg,
        buf[0],         // dst
        &adc_hw->fifo,  // src
        BUF_LEN,        // transfer count
        false           // start immediately
    );

    // DMA irq
    dma_channel_set_irq1_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_1, dma_handler);
    irq_set_priority(DMA_IRQ_1, 0x40); // high priority
    irq_set_enabled(DMA_IRQ_1, true);

    // initialize semaphore
    sem_init(&sem, 0, BUF_NUM);

    //tnc_init();
    //bell202_init();

    // DMA start
    dma_channel_start(dma_chan);

    // ADC start
    adc_run(true);
}

void receive(void)
{
    static int buf_next = 0;
    static uint8_t port = 0;

    // wait for ADC samples
    if (!sem_acquire_timeout_ms(&sem, 0)) return;

#ifdef BUSY_PIN
    gpio_put(BUSY_PIN, 1);
#endif

    ++__tnc_time; // advance 10ms timer

    // process adc data
    for (int i = 0; i < BUF_LEN; i++) {
        int val = buf[buf_next][i];
        tnc_t *tp = &tnc[port];

        if (++port >= PORT_N) port = 0; // ADC ch round robin

        // decode Bell202
#if 0
#if ADC_BIT == 8
        demodulator(tp, val - 128);
#else
        demodulator(tp, val - 2048);
#endif
#else
        demodulator(tp, val); // pass raw value
#endif

    }

    // advance next buffer
    ++buf_next;
    buf_next &= BUF_NUM - 1;

#ifdef BUSY_PIN
    gpio_put(BUSY_PIN, 0);
#endif
}

void receive_off(void)
{
    adc_run(false);
}

void receive_on(void)
{
    adc_run(true);
}
