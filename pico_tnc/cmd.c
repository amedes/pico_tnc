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
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "class/cdc/cdc_device.h"
#include "pico/sync.h"

#include "usb_output.h"
#include "usb_input.h"
#include "ax25.h"
#include "tnc.h"
#include "tty.h"
#include "flash.h"
#include "receive.h"
#include "beacon.h"

typedef struct CMD {
    uint8_t *name;
    int len;
    bool (*func)(tty_t *ttyp, uint8_t *buf, int len);
} cmd_t;

static char const help_str[] =
    "\r\n"
    "Commands are Case Insensitive\r\n"
    "Use Backspace Key (BS) for Correction\r\n"
    "Use the DISP command to desplay all options\r\n"
    "Insert Jumper J4 and Connect GPS for APRS Operation\r\n"
    "Insert Jumper J5 and Connect to Terminal for Command Interpreter\r\n"
    "\r\n"
    "Commands (with example):\r\n"
    "MYCALL (mycall jn1dff-2)\r\n"
    "UNPROTO (unproto jn1dff-14 v jn1dff-1) - 3 digis max\r\n"
    "BTEXT (btext Bob)-100 chars max\r\n"
    "BEACON (beacon every n)- n=0 is off and 1<n<60 mins\r\n"
    "MONitor (mon all,mon me, or mon off)\r\n"
    "DIGIpeat (digi on or digi off)\r\n"
    "MYALIAS (myalias RELAY)\r\n"
    "PERM (PERM)\r\n"
    "ECHO (echo on or echo off)\r\n"
    "GPS (gps $GPGAA or gps $GPGLL or gps $GPRMC)\r\n"
    "TRace (tr xmit or tr rcv) - For debugging only\r\n"
    "TXDELAY (txdelay n 0<n<201 n is number of delay flags to send)\r\n"
    "CALIBRATE (Calibrate Mode - Testing Only)\r\n"
    "\r\n";



enum STATE_CALLSIGN {
    CALL = 0,
    HYPHEN,
    SSID1,
    SSID2,
    SPACE,
    END,
};



enum TRACE {
    TR_OFF = 0,
    TR_XMIT,
    TR_RCV,
};

static const uint8_t *gps_str[] = {
    "$GPGGA",
    "$GPGLL",
    "$GPRMC",
};

// indicate converse mode
bool converse_mode = false;

static uint8_t *read_call(uint8_t *buf, callsign_t *c)
{
    callsign_t cs;
    int i, j;
    int state = CALL;
    bool error = false;

    cs.call[i] = '\0';
    for (i = 1; i < 6; i++) cs.call[i] = ' ';
    cs.ssid = 0;

    // callsign
    j = 0;
    for (i = 0; buf[i] && state != END; i++) {
        int ch = buf[i];

        switch (state) {

            case CALL:
                if (isalnum(ch)) {
                    cs.call[j++] = toupper(ch);
                    if (j >= 6) state = HYPHEN;
                    break;
                } else if (ch == '-') {
                    state = SSID1;
                    break;
                } else if (ch != ' ') {
                    error = true;
                }
                state = END;
                break;

            case HYPHEN:
                if (ch == '-') {
                    state = SSID1;
                    break;
                }
                if (ch != ' ') {
                    error = true;
                }
                state = END;
                break;

            case SSID1:
                if (isdigit(ch)) {
                    cs.ssid = ch - '0';
                    state = SSID2;
                    break;
                }
                error = true;
                state = END;
                break;

            case SSID2:
                if (isdigit(ch)) {
                    cs.ssid *= 10;
                    cs.ssid += ch - '0';
                    state = SPACE;
                    break;
                }
                /* FALLTHROUGH */

            case SPACE:
                if (ch != ' ') error = true;
                state = END;
        }
    }

    if (cs.ssid > 15) error = true;

    if (error) return NULL;

    memcpy(c, &cs, sizeof(cs));

    return &buf[i];
}

static int callsign2ascii(uint8_t *buf, callsign_t *c)
{
    int i;

    if (!c->call[0]) {
        memcpy(buf, "NOCALL", 7);
        
        return 6;
    }

    for (i = 0; i < 6; i++) {
        int ch = c->call[i];

        if (ch == ' ') break;

        buf[i] = ch;
    }

    if (c->ssid > 0) {
        buf[i++] = '-';

        if (c->ssid > 9) {
            buf[i++] = '1';
            buf[i++] = c->ssid - 10 + '0';
        } else {
            buf[i++] = c->ssid + '0';
        }
    }

    buf[i] = '\0';

    return i;
}

static bool cmd_mycall(tty_t *ttyp, uint8_t *buf, int len)
{
    if (buf && buf[0]) {

        return read_call(buf, &param.mycall) != NULL;

        //usb_write(buf, len);
        //usb_write("\r\n", 2);

    } else {
        uint8_t temp[10];

        tty_write_str(ttyp, "MYCALL ");
        tty_write(ttyp, temp, callsign2ascii(temp, &param.mycall));
        tty_write_str(ttyp, "\r\n");
    }

    return true;
}

static bool cmd_unproto(tty_t *ttyp, uint8_t *buf, int len)
{
    int i;
    uint8_t *p;


    if (buf && buf[0]) {

        p = read_call(buf, &param.unproto[0]);
        if (p == NULL) return false;

        for (i = 1; i < UNPROTO_N; i++) param.unproto[i].call[0] = '\0';

        for (i = 1; *p && i < 4; i++) {

            while (*p == ' ') p++;

            if (toupper(*p) != 'V') return false;
            p++;
            if (*p != ' ') return false;

            while (*p == ' ') p++;

            p = read_call(p, &param.unproto[i]);
            if (p == NULL) return false;
        }

    } else {

        tty_write_str(ttyp, "UNPROTO ");

        for (i = 0; i < 4; i++) {
            uint8_t temp[10];

            if (!param.unproto[i].call[0]) break;

            if (i > 0) tty_write_str(ttyp, " V ");
            tty_write(ttyp, temp, callsign2ascii(temp, &param.unproto[i]));
        }

        tty_write_str(ttyp, "\r\n");
    }

    return true;
}

static bool cmd_btext(tty_t *ttyp, uint8_t *buf, int len)
{
    if (buf && buf[0]) {

        uint8_t *p = buf;
        int i;

        if (buf[0] == '%' && len == 1) {
            param.btext[0] = '\0';
            return true;
        }

        for (i = 0; i < len && i < BTEXT_LEN; i++) {
            param.btext[i] = buf[i];
        }
        param.btext[i] = '\0';

    } else {

        tty_write_str(ttyp, "BTEXT ");
        tty_write_str(ttyp, param.btext);
        tty_write_str(ttyp, "\r\n");
    }

    return true;
}

static bool cmd_beacon(tty_t *ttyp, uint8_t *buf, int len)
{
    if (buf && buf[0]) {

        static uint8_t const every[] = "EVERY";
        uint8_t const *s = every;
        int i = 0;

        if (!strncasecmp(buf, "OFF", 3)) {
            param.beacon = 0;
            return true;
        }

        while (toupper(buf[i]) == *s) {
            i++;
            s++;
        }

        if (!buf[i] || buf[i] != ' ') return false;

        int r, t;
        r = sscanf(&buf[i], "%d", &t);

        if (r != 1 || (t < 0 || t > 60)) return false;

        param.beacon = t;
        beacon_reset();     // beacon timer reset

    } else {

        tty_write_str(ttyp, "BEACON ");

        if (param.beacon > 0) {
            uint8_t temp[4];

            tty_write_str(ttyp, "On EVERY ");
            tty_write(ttyp, temp, sprintf(temp, "%u", param.beacon));
        } else {
            tty_write_str(ttyp, "Off");
        }
        tty_write_str(ttyp, "\r\n");
    }

    return true;
}

static bool cmd_monitor(tty_t *ttyp, uint8_t *buf, int len)
{
    if (buf && buf[0]) {

        if (!strncasecmp(buf, "ALL", 3)) {
            param.mon = MON_ALL;
        } else if (!strncasecmp(buf, "ME", 2)) {
            param.mon = MON_ME;
        } else if (!strncasecmp(buf, "OFF", 3)) {
            param.mon = MON_OFF;
        } else {
            return false;
        }

    } else {

        tty_write_str(ttyp, "MONitor ");
        if (param.mon == MON_ALL) {
            tty_write_str(ttyp, "ALL");
        } else if (param.mon == MON_ME) {
            tty_write_str(ttyp, "ME");
        } else {
            tty_write_str(ttyp, "OFF");
        }
        tty_write_str(ttyp, "\r\n");

    }

    return true;
}

static bool cmd_digipeat(tty_t *ttyp, uint8_t *buf, int len)
{
    if (buf && buf[0]) {

        if (!strncasecmp(buf, "ON", 2)) {
            param.digi = true;
        } else if (!strncasecmp(buf, "OFF", 3)) {
            param.digi = false;
        } else {
            return false;
        }

    } else {

        tty_write_str(ttyp, "DIGIpeater ");
        if (param.digi) {
            tty_write_str(ttyp, "ON");
        } else {
            tty_write_str(ttyp, "OFF");
        }
        tty_write_str(ttyp, "\r\n");
    }

    return true;
}

static bool cmd_myalias(tty_t *ttyp, uint8_t *buf, int len)
{
    if (buf && buf[0]) {

        return read_call(buf, &param.myalias) != NULL;

        //usb_write(buf, len);
        //usb_write("\r\n", 2);

    } else {
        uint8_t call[10];

        tty_write_str(ttyp, "MYALIAS ");
        if (param.myalias.call[0]) tty_write(ttyp, call, callsign2ascii(call, &param.myalias));
        tty_write_str(ttyp, "\r\n");

    }

    return true;
}

static bool cmd_perm(tty_t *ttyp, uint8_t *buf, int len)
{
    //tty_write("PERM\r\n", 6);

    receive_off(); // stop ADC free running

    bool ret = flash_write(&param, sizeof(param));

    receive_on();

    return ret;
}

static bool cmd_echo(tty_t *ttyp, uint8_t *buf, int len)
{
    if (buf && buf[0]) {

        if (!strncasecmp(buf, "ON", 2)) {
            param.echo = 1;
        } else if (!strncasecmp(buf, "OFF", 3)) {
            param.echo = 0;
        } else {
            return false;
        }

     } else {

        tty_write_str(ttyp, "ECHO ");
        if (param.echo) {
            tty_write_str(ttyp, "ON"); 
        } else {
            tty_write_str(ttyp, "OFF");
        }
        tty_write_str(ttyp, "\r\n");
    }

    return true;
}

static bool cmd_gps(tty_t *ttyp, uint8_t *buf, int len)
{
    if (buf && buf[0]) {

        for (int i = 0; i < 3; i++) {
            uint8_t const *str = gps_str[i];
        
            if (!strncasecmp(buf, str, strlen(str))) {
                param.gps = i;
                return true;
            }
        }
        return false;

    } else {

        tty_write_str(ttyp, "GPS ");
        tty_write_str(ttyp, gps_str[param.gps]);
        tty_write_str(ttyp, "\r\n");
    }

    return true;
}

static bool cmd_trace(tty_t *ttyp, uint8_t *buf, int len)
{
    if (buf && buf[0]) {

        if (!strncasecmp(buf, "OFF", 3)) {
            param.trace = TR_OFF;
        } else if (!strncasecmp(buf, "XMIT", 4)) {
            param.trace = TR_XMIT;
        } else if (!strncasecmp(buf, "RCV", 3)) {
            param.trace = TR_RCV;
        } else {
            return false;
        }
    
    } else {

        tty_write_str(ttyp, "TRace ");
        if (param.trace == TR_XMIT) {
            tty_write_str(ttyp, "XMIT");
        } else if (param.trace == TR_RCV) {
            tty_write_str(ttyp, "RCV");
        } else {
            tty_write_str(ttyp, "OFF");
        }
        tty_write_str(ttyp, "\r\n");
    }
    
    return true;
}

static bool cmd_txdelay(tty_t *ttyp, uint8_t *buf, int len)
{
    if (buf && buf[0]) {
        
        int t = atoi(buf);

        if (t <= 0 || t > 200) return false;

        param.txdelay = t;

        // set txdelay
        tnc[0].kiss_txdelay = param.txdelay * 2 / 3;

    } else {
        uint8_t temp[8];

        tty_write_str(ttyp, "TXDELAY ");
        tty_write(ttyp, temp, snprintf(temp, 8, "%u", param.txdelay));
        tty_write_str(ttyp, "\r\n");
    }

    return true;
}

static bool cmd_calibrate(tty_t *ttyp, uint8_t *buf, int len)
{
    tty_write_str(ttyp, "CALIBRATE\r\n");
}

static bool cmd_converse(tty_t *ttyp, uint8_t *buf, int len)
{
    //tty_write("CONVERSE\r\n", 10);
    converse_mode = true;
    tty_write_str(ttyp, "***  Converse Mode, ctl C to Exit\r\n");
    return true;
}

static bool cmd_kiss(tty_t *ttyp, uint8_t *buf, int len)
{
    if (buf && buf[0]) {

        if (!strncasecmp(buf, "ON", 2)) {
            ttyp->kiss_mode = 1;
        } else if (!strncasecmp(buf, "OFF", 3)) {
            ttyp->kiss_mode = 0;
        } else {
            return false;
        }

     } else {

        tty_write_str(ttyp, "KISS ");
        if (ttyp->kiss_mode) {
            tty_write_str(ttyp, "ON"); 
        } else {
            tty_write_str(ttyp, "OFF");
        }
        tty_write_str(ttyp, "\r\n");
    }

    return true;
}

static bool cmd_help(tty_t *ttyp, uint8_t *buf, int len)
{
    //printf("tud_cdc_write_available() = %d\n", tud_cdc_write_available());

    tty_write_str(ttyp, help_str);

    //printf("tud_cdc_write_available() = %d\n", tud_cdc_write_available());

    return true;
}

static bool cmd_disp(tty_t *ttyp, uint8_t *buf, int len)
{
    uint8_t temp[10]; // 6 + '-' + 2 + '\0'

    tty_write_str(ttyp, "\r\n");

    // echo
    cmd_echo(ttyp, NULL, 0);

    // txdelay
    cmd_txdelay(ttyp, NULL, 0);

    // gps
    cmd_gps(ttyp, NULL, 0);

    // trace
    cmd_trace(ttyp, NULL, 0);

    // monitor
    cmd_monitor(ttyp, NULL, 0);

    // digipeat
    cmd_digipeat(ttyp, NULL, 0);

    // beacon
    cmd_beacon(ttyp, NULL, 0);

    // unproto
    cmd_unproto(ttyp, NULL, 0);

    // mycall
    cmd_mycall(ttyp, NULL, 0);

    // myalias
    cmd_myalias(ttyp, NULL, 0);

    // btext
    cmd_btext(ttyp, NULL, 0);

    //usb_write("\r\n", 2);
    
    return true;
}

static const cmd_t cmd_list[] = {
    { "HELP", 4, cmd_help, },
    { "?", 1, cmd_help, },
    { "DISP", 4, cmd_disp, },
    { "MYCALL", 6, cmd_mycall, },
    { "UNPROTO", 7, cmd_unproto, },
    { "BTEXT", 6, cmd_btext, },
    { "BEACON", 7, cmd_beacon, },
    { "MONITOR", 8, cmd_monitor, },
    { "DIGIPEAT", 9, cmd_digipeat, },
    { "MYALIAS", 8, cmd_myalias, },
    { "PERM", 4, cmd_perm, },
    { "ECHO", 4, cmd_echo, },
    { "GPS", 3, cmd_gps, },
    { "TRACE", 5, cmd_trace, },
    { "TXDELAY", 7, cmd_txdelay, },
    { "CALIBRATE", 9, cmd_calibrate, },
    { "CONVERSE", 8, cmd_converse, },
    { "K", 1, cmd_converse, },
    { "KISS", 4, cmd_kiss, },

    // end mark
    { NULL, 0, NULL, },
};


void cmd(tty_t *ttyp, uint8_t *buf, int len)
{
#if 0
    tud_cdc_write(buf, len);
    tud_cdc_write("\r\n", 2);
    tud_cdc_write_flush();
#endif

    uint8_t *top;
    int i;

    for (i = 0; i < len; i++) {
        if (buf[i] != ' ') break;
    }
    top = &buf[i];
    int n = len - i;

    if (n <= 0) return;

    uint8_t *param = strchr(top, ' ');
    int param_len = 0;

    if (param) {
        n = param - top;
        param_len = len - (param - buf);

        for (i = 0; i < param_len; i++) {
            if (param[i] != ' ') break;
        }
        param += i;
        param_len -= i;
    }

    cmd_t const *cp = &cmd_list[0], *mp;
    int matched = 0;

    while (cp->name) {

#if 0
        tud_cdc_write(cp->name, cp->len);
        tud_cdc_write("\r\n", 2);
        tud_cdc_write_flush();
#endif
     
        if (cp->len >= n && !strncasecmp(top, cp->name, n)) {
            ++matched;
            mp = cp;
        }
        cp++;
    }

    if (matched == 1) {

        if (mp->func(ttyp, param, param_len)) {
            if (!converse_mode) tty_write_str(ttyp, "\r\nOK\r\n");
            return;
        }
    }

    tty_write_str(ttyp, "\r\n?\r\n");
}
