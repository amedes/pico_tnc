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
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/structs/uart.h"
#include "pico/util/queue.h"
#include "hardware/watchdog.h"

#include "tnc.h"
#include "receive.h"
#include "send.h"
#include "ax25.h"

//#define TEST_PACKET 1

#ifdef TEST_PACKET
#include "test.h"
//#include "packet_table.h"
#endif

#include "cmd.h"
//#include "usb_input.h"
#include "usb_output.h"
#include "serial.h"
#include "tty.h"
#include "beacon.h"

#define TIME_10MS (10 * 1000)    // 10 ms = 10 * 1000 us

// greeting message
static const uint8_t greeting[] =
    "\r\nJN1DFF MODELESS TNC  V 1.00\r\n"
    "Type HELP for Info\r\n"
    "\r\n"
    "cmd: ";

int main()
{
    stdio_init_all();

    if (watchdog_caused_reboot()) {
        printf("Watch Dog Timer Failure\n");
    }

    // create usb output queue
    usb_output_init();

    // initialize tnc
    tnc_init();
    send_init();
    receive_init();
    serial_init();
    tty_init();     // should call after tnc_init()
    //bell202_init();
#ifdef TEST_PACKET
    //test_init((1 << PORT_N) - 1); // test packet for all port
    test_init(1); // only port 0
#endif

#ifdef BUSY_PIN
    gpio_init(BUSY_PIN);
    gpio_set_dir(BUSY_PIN, true); // output
#endif

#define SMPS_PIN 23
#if 1
    gpio_init(SMPS_PIN);
    gpio_set_dir(SMPS_PIN, true); // output
    gpio_put(SMPS_PIN, 0);
#endif

    // output greeting text
    tty_write_str(&tty[0], greeting);
    tty_write_str(&tty[1], greeting);

    //uint32_t ts = time_us_32();

    // set watchdog, timeout 1000 ms
    watchdog_enable(1000, true);

    // main loop
    while (1) {

        // update watchdog timer
        watchdog_update();

#if 0
        // advance tnc time
        if (time_us_32() - ts >= TIME_10MS) {
            ++tnc_time;
            ts += TIME_10MS;
        }
#endif

        // receive packet
        receive();

        // send packet
        send();

        // incoming KISS frame to serial
        //kiss_input();

        // output KISS frame to serial
        //kiss_output();

        // process uart I/O
        serial_input();
        serial_output();

#ifdef TEST_PACKET
        // send test packet
        test();
#endif

        // send beacon
        beacon();

#ifdef BUSY_PIN
//        gpio_put(BUSY_PIN, 0);
#endif
        // wait small time
        __wfi();

#ifdef BUSY_PIN
//        gpio_put(BUSY_PIN, 1);
#endif

    }

    return 0;
}
