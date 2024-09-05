// The MIT License (MIT)
//
// Copyright (c) 2024 embeddedboys developers
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <stdio.h>

#include "pico/time.h"
#include "pico/stdlib.h"
#include "pico/platform.h"

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"

#include "boards/pico.h"

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"

#include "i80.pio.h"

#define DEBUG 0
#if DEBUG
    #define pr_debug(...) mp_printf(&mp_plat_print, __VA_ARGS__)
#else
    #define pr_debug(...)
#endif

#ifndef PIO_USE_DMA
    #define PIO_USE_DMA 1
#endif
struct i80_data {
    /* Pins for 8080 PIO */
    uint db_base;   /* The base pin of 8080 data bus */
    uint db_count;  /* The total count of 8080 data bus from base */
    uint pin_wr;    /* Pin number of WR signal */

    /* PIO self things */
    PIO pio;    /* which pio instance */
    uint sm;    /* which state machine will be used */
    uint offset;    /* offset of PIO program */
    float clk_div;

    /* DMA things */
    uint dma_tx;    /* DMA channel */
    dma_channel_config dma_chnn_cfg;
} g_i80;

static void __time_critical_func(i80_set_rs_cs)(bool rs, bool cs)
{
    gpio_put_masked((1u << LCD_PIN_RS) | (1u << LCD_PIN_CS), !!rs << LCD_PIN_RS | !!cs << LCD_PIN_CS);
}

static void __time_critical_func(i80_set_rs)(bool rs)
{
    gpio_put(LCD_PIN_RS, rs);
}

#if PIO_USE_DMA
#define define_i80_write_piox(func, buffer_type) \
void func(PIO pio, uint sm, void *buf, size_t len) \
{ \
    dma_channel_configure(g_i80.dma_tx, &g_i80.dma_chnn_cfg,   \
                          &pio->txf[sm],    \
                          (buffer_type *)buf,  \
                          len / sizeof(buffer_type),  \
                          true  \
    );  \
    dma_channel_wait_for_finish_blocking(g_i80.dma_tx);   \
}
#else
#define define_i80_write_piox(func, buffer_type) \
void func(PIO pio, uint sm, void *buf, size_t len) \
{ \
    buffer_type data;   \
    \
    i80_wait_idle(pio, sm); \
    while (len) {   \
        data = *(buffer_type *)buf; \
    \
        i80_put(pio, sm, data); \
    \
        buf += sizeof(buffer_type);   \
        len -= sizeof(buffer_type);   \
    }   \
    i80_wait_idle(pio, sm); \
}
#endif

define_i80_write_piox(i80_write_pio8, uint8_t)
define_i80_write_piox(i80_write_pio16, uint16_t)

int __time_critical_func(i80_write_buf_rs)(void *buf, size_t len, bool rs)
{
    i80_wait_idle(g_i80.pio, g_i80.sm);
    i80_set_rs_cs(rs, 0);

    switch (g_i80.db_count) {
    case 8:
        i80_write_pio8(g_i80.pio, g_i80.sm, buf, len);
        break;
    case 16:
        i80_write_pio16(g_i80.pio, g_i80.sm, buf, len);
        break;
    default:
        printf("invaild data bus width\n");
        break;
    }

    i80_wait_idle(g_i80.pio, g_i80.sm);
    i80_set_rs_cs(rs, 0);
    return 0;
}

int i80_pio_init(uint8_t db_base, uint8_t db_count, uint8_t pin_wr)
{
    // printf("i80 PIO initialzing...\n");

    g_i80.db_base = db_base;
    g_i80.db_count = db_count;
    g_i80.pin_wr = pin_wr;

    g_i80.pio = pio0;
    g_i80.sm = 0;

#if PIO_USE_DMA
    g_i80.dma_tx = dma_claim_unused_channel(true);
    g_i80.dma_chnn_cfg = dma_channel_get_default_config(g_i80.dma_tx);

    switch (g_i80.db_count) {
    case 8:
        channel_config_set_transfer_data_size(&g_i80.dma_chnn_cfg, DMA_SIZE_8);
        break;
    case 16:
        channel_config_set_transfer_data_size(&g_i80.dma_chnn_cfg, DMA_SIZE_16);
        break;
    default:
        // printf("invaild data bus width\n");
        goto exit_release_chnn;
        break;
    }

    channel_config_set_dreq(&g_i80.dma_chnn_cfg, pio_get_dreq(g_i80.pio, g_i80.sm, true));
#endif

    g_i80.offset = pio_add_program(g_i80.pio, &i80_program);
    g_i80.clk_div = ((clock_get_hz(clk_sys) / 1000.f) / 2.f / I80_BUS_CLK_KHZ);

    i80_program_init(
        g_i80.pio, g_i80.sm, g_i80.offset,
        g_i80.db_base, g_i80.db_count,
        g_i80.pin_wr, g_i80.clk_div
    );

    return 0;

exit_release_chnn:
    dma_channel_unclaim(g_i80.dma_tx);
    return -1;
}
