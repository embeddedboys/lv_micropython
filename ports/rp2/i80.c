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

#define USE_DMA 1
#define I80_BUS_CLK_KHZ 50000
#define LCD_PIN_RS 20

static PIO g_pio = pio0;
static uint g_sm = 0;

void foo(void)
{
    mp_printf(&mp_plat_print, "%s\n", __func__);
}

// void __time_critical_func(i80_set_rs_cs)(bool rs, bool cs)
// {
//     gpio_put_masked((1u << LCD_PIN_RS) | (1u << LCD_PIN_CS), !!rs << LCD_PIN_RS | !!cs << LCD_PIN_CS);
// }

void __time_critical_func(i80_set_rs)(bool rs)
{
    gpio_put_masked(1u << LCD_PIN_RS, !!rs << LCD_PIN_RS);
}

#if USE_DMA
/* DMA version */
static uint dma_tx;
static dma_channel_config c;
static inline void __time_critical_func(i80_write_pio16_wr)(PIO pio, uint sm, void *buf, size_t len)
{
    dma_channel_configure(dma_tx, &c,
                          &pio->txf[sm], /* write address */
                          (uint16_t *)buf, /* read address */
                          len / 2, /* element count (each element is of size transfer_data_size) */
                          true /* start right now */
    );

    // dma_start_channel_mask(1u << dma_tx);

    /* TODO: use another core to wait. */
    dma_channel_wait_for_finish_blocking(dma_tx);
}
#else
static inline int i80_write_pio16_wr(PIO pio, uint sm, void *buf, size_t len)
{
    uint16_t data;

    i80_wait_idle(pio, sm);
    while (len) {
        data = *(uint16_t *)buf;

        i80_put(pio, sm, data);

        buf += 2;
        len -= 2;
    }
    i80_wait_idle(pio, sm);
    return 0;
}
#endif

void __time_critical_func(i80_write_buf_rs)(void *buf, size_t len, bool rs)
{
    i80_set_rs(rs);
    i80_write_pio16_wr(g_pio, g_sm, buf, len);
}

int i80_pio_init(uint8_t db_base, uint8_t db_count, uint8_t pin_wr)
{
    pr_debug("i80 PIO initialzing...\n");

#if USE_DMA
    dma_tx = dma_claim_unused_channel(true);
    c = dma_channel_get_default_config(dma_tx);

    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, pio_get_dreq(g_pio, g_sm, true));
#endif

    uint offset = pio_add_program(g_pio, &i80_program);
    float clk_div = ((clock_get_hz(clk_sys) / 1000.f) / 2.f / I80_BUS_CLK_KHZ);
    pr_debug("clk_div : %f\n", clk_div);
    i80_program_init(g_pio, g_sm, offset, db_base, db_count, pin_wr, clk_div);

    return 0;
}

