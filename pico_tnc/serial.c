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
#include "hardware/uart.h"
#include "pico/util/queue.h"

#include "tnc.h"
#include "tty.h"
#include "gps.h"

#define UART_BAUDRATE 115200
#define UART_QUEUE_LEN 1024

#define GPS_ENABLE 1

#define GPS_BAUDRATE 9600

static queue_t uart_queue;

void serial_init(void)
{
    queue_init(&uart_queue, sizeof(uint8_t), UART_QUEUE_LEN);
    assert(uart_queue != NULL);

    uint baud = uart_init(uart0, UART_BAUDRATE);

    //printf("UART0 baud rate = %u\n", baud);

    uart_set_fifo_enabled(uart0, true);

    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);

#ifdef GPS_ENABLE
    // GPS
    baud = uart_init(uart1, GPS_BAUDRATE);

    //printf("UART1 baud rate = %u\n", baud);

    uart_set_fifo_enabled(uart0, true);
    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);
#endif
}

void serial_write(uint8_t const *data, int len)
{
    int free = UART_QUEUE_LEN - queue_get_level(&uart_queue);

    for (int i = 0; i < len && i < free; i++) {
        if (!queue_try_add(&uart_queue, &data[i])) break;
    }
}

void serial_write_char(uint8_t ch)
{
    queue_try_add(&uart_queue, &ch);
}

void serial_output(void)
{
    if (queue_is_empty(&uart_queue)) return;

    while (uart_is_writable(uart0)) {
        uint8_t ch;

        if (!queue_try_remove(&uart_queue, &ch)) break;
        uart_putc_raw(uart0, ch);
    }
}

void serial_input(void)
{
#ifdef GPS_ENABLE
    while (uart_is_readable(uart1)) {
        int ch = uart_getc(uart1);
        gps_input(ch);
    }
#endif

    while (uart_is_readable(uart0)) {

        int ch = uart_getc(uart0);
        tty_input(&tty[TTY_UART0], ch);
    }
#if 0
    switch (ch) {
        case 0x08:
        case 0x7f:
            uart_puts(uart0, "\b \b");
            break;

        case 0x0d:
            uart_puts(uart0, "\r\n");
            break;

        default:
            uart_putc_raw(uart0, ch);
    }
#endif
}
