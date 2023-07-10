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
    calculate CCITT-16 CRC using DMA CRC hardware
*/
#include <stdio.h>
#include "pico/stdlib.h"

#include "ax25.h"

//#ifdef RASPBERRYPI_PICO
#ifdef PICO_DEFAULT_UART

#include "hardware/dma.h"

#define OUT_INV (1 << 11)
#define OUT_REV (1 << 10)

int ax25_fcs(uint32_t crc, const uint8_t *data, int size)
{
    uint8_t dummy;
    int dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_write_increment(&c, false);
    channel_config_set_read_increment(&c, true);
    channel_config_set_dreq(&c, DREQ_FORCE);

    dma_channel_configure(
        dma_chan,
        &c,
        &dummy,
        data,
        size,
        false
    );

    // enable sniffer
    dma_sniffer_enable(dma_chan, 0x3, true);
    dma_hw->sniff_ctrl |= OUT_INV | OUT_REV;
    dma_hw->sniff_data = crc;
    dma_hw->sniff_data >>= 16;

    // start DMA
    dma_channel_start(dma_chan);

    // wait for finish
    dma_channel_wait_for_finish_blocking(dma_chan);
    dma_channel_unclaim(dma_chan);

    return dma_hw->sniff_data >> 16;
}

#else

#warning using C standard routine

#define CRC16_POLY 0x10811 /* G(x) = 1 + x^5 + x^12 + x^16 */

int ax25_fcs(uint32_t crc, const uint8_t packet[], int length)
{
    int i, j;

    if (length <= 0) return -1; // packet too short

    // calculate CRC x^16 + x^12 + x^5 + 1
    crc = 0xffff; /* initial value */
    for (i = 0; i < length; i++) {
	crc ^= packet[i];
	for (j = 0; j < 8; j++) {
	    if (crc & 1) crc ^= CRC16_POLY;
	    crc >>= 1;
	}
    }
    crc ^= 0xffff; // invert

    return crc;
}

#endif

bool ax25_callcmp(callsign_t *c, uint8_t *addr)
{
    for (int i = 0; i < 6; i++) {
        if (c->call[i] != (addr[i] >> 1)) return false;
    }

    if (c->ssid == ((addr[6] >> 1) & 0x0f)) return true;

    return false;
}

void ax25_mkax25addr(uint8_t *addr, callsign_t *c)
{
    uint8_t *s = addr;

    for (int i = 0; i < 6; i++) {
        *s++ = c->call[i] << 1;
    }

    // SSID
    *s = (c->ssid << 1) | 0x60;
}

bool ax25_ui(uint8_t *packet, int len)
{
    int i;

    i = AX25_ADDR_LEN - 1; // SSID
    while (i < len) {
        if (packet[i] & 1) break; // address extension bit
        i += AX25_ADDR_LEN;
    }
    i++;

    if (i + 2 > len) return false; // no control and  PID field

    return packet[i] == 0x03 && packet[i+1] == 0xf0; // true if UI packet
}
