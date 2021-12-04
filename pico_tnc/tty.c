/*
Copyright (c) 2021, JN1DFF
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
#include "class/cdc/cdc_device.h"
#include "pico/sync.h"
#include "hardware/uart.h"

#include "tnc.h"
#include "cmd.h"
#include "usb_output.h"
#include "tnc.h"
#include "usb_input.h"
#include "serial.h"
#include "unproto.h"
#include "kiss.h"

#define CONVERSE_PORT 0

// usb echo flag
//uint8_t usb_echo = 1; // on

// tty info
tty_t tty[TTY_N];

//static uint8_t cmd_buf[CMD_LEN + 1];
//static int cmd_idx = 0;

static const enum TTY_MODE tty_mode[] = {
    TTY_TERMINAL,
    TTY_TERMINAL,
    TTY_GPS,
};

static const enum TTY_SERIAL tty_serial[] = {
    TTY_USB,
    TTY_UART0,
    TTY_UART1,
};

void tty_init(void)
{
    for (int i = 0; i < TTY_N; i++) {
        tty_t *ttyp = &tty[i];

        ttyp->num = i;

        ttyp->tty_mode = tty_mode[i];
        ttyp->tty_serial = tty_serial[i];

        ttyp->kiss_mode = false;
    }
}

void tty_write(tty_t *ttyp, uint8_t const *data, int len)
{
    if (ttyp->tty_serial == TTY_USB) {
        usb_write(data, len);
        return;
    }

    if (ttyp->tty_serial == TTY_UART0) serial_write(data, len);
}

void tty_write_char(tty_t *ttyp, uint8_t ch)
{
    if (ttyp->tty_serial == TTY_USB) {
        usb_write_char(ch);
        return;
    }

    if (ttyp->tty_serial == TTY_UART0) serial_write_char(ch);
}

void tty_write_str(tty_t *ttyp, uint8_t const *str)
{
    int len = strlen(str);

    tty_write(ttyp, str, len);
}

#define BS '\b'
#define CR '\r'
#define DEL '\x7f'
#define BELL '\a'
#define CTRL_C '\x03'
#define FEND 0xc0
#define SP ' '

#define KISS_TIMEOUT (1 * 100) // 1 sec
 
#define CAL_DATA_MAX 3

static const uint8_t calibrate_data[CAL_DATA_MAX] = {
    0x00, 0xff, 0x55,
};

static const char *calibrate_str[CAL_DATA_MAX] = {
    "send space (2200Hz)\r\n",
    "send mark  (1200Hz)\r\n",
    "send 0x55  (1200/2200Hz)\r\n",
};

void tty_input(tty_t *ttyp, int ch)
{
    if (ttyp->kiss_state != KISS_OUTSIDE) {

        // inside KISS frame
        if (tnc_time() - ttyp->kiss_timeout < KISS_TIMEOUT) {
            kiss_input(ttyp, ch);
            return;
        }
        // timeout, exit kiss frame
        ttyp->kiss_state = KISS_OUTSIDE;
    }

    // calibrate mode
    if (calibrate_mode) {
        tnc_t *tp = &tnc[0];

        switch (ch) {
            case SP: // toggle mark/space
                if (++calibrate_idx >= CAL_DATA_MAX) calibrate_idx = 0;
                tp->cal_data = calibrate_data[calibrate_idx];
                tty_write_str(ttyp, calibrate_str[calibrate_idx]);
                tp->cal_time = tnc_time();
                break;

            case CTRL_C:
                tp->send_state = SP_CALIBRATE_OFF;
                break;

            default:
                tty_write_char(ttyp, BELL);
        }
        return;
    }

    switch (ch) {
        case FEND: // KISS frame end
            kiss_input(ttyp, ch);
            break;

        case BS:
        case DEL:
            if (ttyp->cmd_idx > 0) {
                --ttyp->cmd_idx;
                if (param.echo) tty_write_str(ttyp, "\b \b");
            } else {
                if (param.echo) tty_write_char(ttyp, BELL);
            }
            break;

        case CR:
            if (param.echo) tty_write_str(ttyp, "\r\n");
            if (ttyp->cmd_idx > 0) {
                ttyp->cmd_buf[ttyp->cmd_idx] = '\0';
                if (converse_mode) {
                    send_unproto(&tnc[CONVERSE_PORT], ttyp->cmd_buf, ttyp->cmd_idx); // send UI packet
                } else {
                    cmd(ttyp, ttyp->cmd_buf, ttyp->cmd_idx);
                }
            }
            if (!(converse_mode | calibrate_mode)) tty_write_str(ttyp, "cmd: ");
            ttyp->cmd_idx = 0;
            break;

        case CTRL_C:
            if (converse_mode) {
                converse_mode = false;
            }
            tty_write_str(ttyp, "\r\ncmd: ");
            ttyp->cmd_idx = 0;
            break;

        default:
            if ((ch >= ' ' && ch <= '~') && ttyp->cmd_idx < CMD_BUF_LEN) {
                ttyp->cmd_buf[ttyp->cmd_idx++] = ch;
            } else {
                ch = BELL;
            }
            if (param.echo) tty_write_char(ttyp, ch);

    }
}
