#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#define PICO_MAGIC 0x4f434950

extern char __flash_binary_end;

static uint8_t *flash_addr(void)
{
    return (uint8_t *)(((uint32_t)&__flash_binary_end + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1));
}

static uint8_t *flash_find_id(void)
{
    uint8_t *addr = flash_addr();
    uint8_t *id = NULL;

    for (int page = 0; page < FLASH_SECTOR_SIZE; page += FLASH_PAGE_SIZE) {
        if (*(uint32_t *)&addr[page] == PICO_MAGIC)  {
            id = &addr[page];
        } else {
            break;
        }
    }
    return id; // return last one
}

static void flash_erase(void)
{
    uint32_t flash_offset = (uint32_t)flash_addr() - XIP_BASE;
    uint32_t int_save = save_and_disable_interrupts();

    busy_wait_us_32(8334); // wait 10 bit period

    flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
    restore_interrupts(int_save);
}

static void flash_program(uint8_t *flash, uint8_t *data)
{
    if (flash < flash_addr()) return;
    if (flash >= (uint8_t *)0x10200000) return; // > 2MB

    uint32_t flash_offset = (uint32_t)flash - XIP_BASE;
    uint32_t int_save = save_and_disable_interrupts();

    busy_wait_us_32(8334); // wait 10 bit period

    flash_range_program(flash_offset, data, FLASH_PAGE_SIZE);
    restore_interrupts(int_save);
}

bool flash_read(void *data, int len)
{
    if (len > FLASH_PAGE_SIZE - sizeof(uint32_t)) return false;

    uint8_t *src = flash_find_id();

    if (src == NULL) return false;

    memcpy(data, src + sizeof(uint32_t), len);
    return true;
}

bool flash_write(void *data, int len)
{
    if (len > FLASH_PAGE_SIZE - sizeof(uint32_t)) return false;

    uint8_t *mem = malloc(FLASH_PAGE_SIZE);

    if (mem == NULL) return false;

    uint32_t id = PICO_MAGIC;

    memcpy(mem, &id, sizeof(id));
    memcpy(mem + sizeof(id), data, len);

    uint8_t *dst = flash_find_id();

    if (!dst) { // flash not initialized

        flash_erase();
        dst = flash_addr();

    } else {

        dst += FLASH_PAGE_SIZE;

        if (dst - flash_addr() >= FLASH_SECTOR_SIZE) { // no writable area

            flash_erase();
            dst = flash_addr();

        }
    }

    flash_program(dst, mem);

    bool matched = !memcmp(dst, mem, FLASH_PAGE_SIZE);

    free(mem);

    return matched;
}
