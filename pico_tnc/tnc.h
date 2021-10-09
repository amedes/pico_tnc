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

#pragma once
#include "pico/util/queue.h"

#include "filter.h"
#include "ax25.h"
//#include "cmd.h"

// number of ports
#define PORT_N 1    // number of ports, 1..3

#define BAUD_RATE 1200
#define SAMPLING_N 11
//#define DELAY_N 3
//#define SAMPLING_RATE ((1000000*DELAY_N+DELAY_US/2)/DELAY_US)
#define SAMPLING_RATE (BAUD_RATE * SAMPLING_N)
#define DELAY_US 446 // 446us
#define DELAYED_N ((SAMPLING_RATE * DELAY_US + 500000) / 1000000)

#define ADC_SAMPLING_RATE (SAMPLING_RATE * PORT_N)

#define DATA_LEN 1024                   // packet receive buffer size

#define FIR_LPF_N 27
#define FIR_BPF_N 25

#define ADC_BIT 8       // adc bits 8 or 12
//#define ADC_BIT 12      // adc bits 8 or 12

//#define BELL202_SYNC 1  // sync decode
#define DECODE_PLL 1    // use PLL

#define CONTROL_N 10
#define DAC_QUEUE_LEN 64
#define DAC_BLOCK_LEN (DAC_QUEUE_LEN + 1)

#define SEND_QUEUE_LEN (1024 * 16)

#define AX25_FLAG 0x7e

#define BUSY_PIN 22

#define BEACON_PORT 0

#define KISS_PACKET_LEN 1024                // kiss packet length
#define TTY_N 3                             // number of serial
#define CMD_BUF_LEN 255


enum STATE {
	FLAG,
	DATA
};

typedef struct {
  int low_i;
  int low_q;
  int high_i;
  int high_q;
} values_t;

typedef struct TNC {
    uint8_t port;

    // receive

    // demodulator
    uint8_t bit;

    // decode_bit
    uint16_t data_cnt;
    uint8_t data[DATA_LEN];
    uint8_t state;
    uint8_t flag;
    uint8_t data_byte;
    uint8_t data_bit_cnt;

    // decode
    uint8_t edge;
    
    // decode2
    int32_t pll_counter;
    uint8_t pval;
    uint8_t nrzi;

    // output_packet
    int pkt_cnt;

    // bell202_decode
    int delayed[DELAYED_N];
    int delay_idx;
    int cdt;
    int cdt_lvl;
    int avg;
    uint8_t cdt_pin;

    // bell202_decode2
    int sum_low_i;
    int sum_low_q;
    int sum_high_i;
    int sum_high_q;
    int low_idx;
    int high_idx;
    values_t values[SAMPLING_N];
    int values_idx;
    filter_t lpf;
    filter_t bpf;

    // send

    // kiss parameter
    uint8_t kiss_txdelay;
    uint8_t kiss_p;
    uint8_t kiss_slottime;
    uint8_t kiss_fullduplex;

    // dac queue
    queue_t dac_queue;

    // DAC, PTT pin
    uint8_t ptt_pin;
    uint8_t pwm_pin;
    uint8_t pwm_slice;

    // DMA channels
    uint8_t ctrl_chan;
    uint8_t data_chan;
    uint32_t data_chan_mask;
    uint8_t busy;

    // Bell202 wave generator
    int next;
    int phase;
    int level;
    int cnt_one;

    // wave buffer for DMA
    uint16_t const *dma_blocks[DAC_BLOCK_LEN][CONTROL_N + 1];

    // send data queue
    queue_t send_queue;
    int send_time;
    int send_len;
    int send_state;
    int send_data;

    // field for test packet
    int test_state;
    uint8_t const *ptp;
    uint8_t const *packet;
    uint16_t packet_len;
    int wait_time;

} tnc_t;

extern tnc_t tnc[];
extern uint32_t __tnc_time;

void tnc_init(void);

inline uint32_t tnc_time(void)
{
    return __tnc_time;
}

// TNC command
enum MONITOR {
    MON_ALL = 0,
    MON_ME,
    MON_OFF,
};

// GPS
enum GPS_SENTENCE {
    GPGGA = 0,
    GPGLL,
    GPRMC,
};

#define UNPROTO_N 4
#define BTEXT_LEN 100


// TNC parameter
typedef struct TNC_PARAM {
    callsign_t mycall;
    callsign_t myalias;
    callsign_t unproto[UNPROTO_N];
    uint8_t btext[BTEXT_LEN + 1];
    uint8_t txdelay;
    uint8_t gps;
    uint8_t mon;
    uint8_t digi;
    uint8_t beacon;
    uint8_t trace;
    uint8_t echo;
} param_t;

extern param_t param;

// tty

enum TTY_MODE {
    TTY_TERMINAL = 0,
    TTY_GPS,
};

enum TTY_SERIAL {
    TTY_USB = 0,
    TTY_UART0,
    TTY_UART1,
};

typedef struct TTY {
    uint8_t kiss_buf[KISS_PACKET_LEN];
    uint8_t cmd_buf[CMD_BUF_LEN + 1];
    int kiss_idx;
    int cmd_idx;

    uint8_t num;        // index of tty[]

    uint8_t tty_mode;   // terminal or GPS
    uint8_t tty_serial; // USB, UART0, UART1

    uint8_t kiss_mode;  // kiss mode
    uint8_t kiss_state; // kiss state
    uint32_t kiss_timeout; // kiss timer

    tnc_t *tp;          // input/output port No.
} tty_t;

extern tty_t tty[];
