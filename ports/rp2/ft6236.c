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

#define pr_fmt(fmt) "ft6236: " fmt

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"

#include "../../lib/lv_bindings/lvgl/lvgl.h"
#include "../../lib/lv_bindings/driver/include/common.h"

#define DRV_NAME "ft6236"
#define pr_debug(...) mp_printf(&mp_plat_print, __VA_ARGS__)

#define FT6236_X_RES 480
#define FT6236_Y_RES 320

#define FT6236_ADDR      0x38
#define FT6236_DEF_SPEED 400000
#define FT6236_PIN_SCL  27
#define FT6236_PIN_SDA  26
#define FT6236_PIN_RST  18

#define CT_MAX_TOUCH  5

#define FT_REG_DEVICE_MODE 	    0x00    // Device mode, 0x00: Normal mode, 0x04: Test mode, 0x03: Factory mode
#define FT_REG_GEST_ID 			0x01    // Gesture ID
#define FT_REG_TD_STATUS 		0x02    // Touch point status

/* TODO: currently support 1 touch point */
#define FT_REG_TOUCH1_XH 		0x03    // Touch point 1 X high 8-bit
#define FT_REG_TOUCH1_XL 		0x04    // Touch point 1 X low 8-bit
#define FT_REG_TOUCH1_YH 		0x05    // Touch point 1 Y high 8-bit
#define FT_REG_TOUCH1_YL 		0x06    // Touch point 1 Y low 8-bit

#define FT_REG_TH_GROUP			0x80
#define FT_REG_PERIODACTIVE	    0x88

#define	FT_REG_LIB_VER_H		0xA1
#define	FT_REG_LIB_VER_L		0xA2
#define FT_REG_CHIPER           0xA3
#define FT_REG_G_MODE 			0xA4
#define FT_REG_FOCALTECH_ID     0xA8
#define FT_REG_RELEASE_CODE_ID  0xAF
#define FT_REG_STATE            0xBC

typedef enum {
    FT6236_DIR_NOP       = 0x00,
    FT6236_DIR_REVERT_X  = 0x01,
    FT6236_DIR_REVERT_Y  = 0x02,
    FT6236_DIR_SWITCH_XY = 0x04,
} ft6236_direction_t;

struct ft6236_data {
    struct {
        uint8_t addr;
        i2c_inst_t *master;
        uint32_t speed;

        uint8_t scl_pin;
        uint8_t sda_pin;
    } i2c;

    uint8_t irq_pin;
    uint8_t rst_pin;

    uint16_t x_res;
    uint16_t y_res;

    ft6236_direction_t dir;   /* direction set */
    bool revert_x;
    bool revert_y;
    uint16_t (*read_x)(struct ft6236_data *priv);
    uint16_t (*read_y)(struct ft6236_data *priv);
} g_ft6236_data;

extern int i2c_bus_scan(i2c_inst_t *i2c);

static int ft6236_write_reg(struct ft6236_data *priv, uint8_t reg, uint8_t val)
{
    uint16_t buf = val << 8 | reg;
    i2c_write_blocking(priv->i2c.master, priv->i2c.addr, (uint8_t *)&buf, sizeof(buf), false);
    return 0;
}
#define write_reg ft6236_write_reg

static uint8_t ft6236_read_reg(struct ft6236_data *priv, uint8_t reg)
{
    uint8_t val;
    i2c_write_blocking(priv->i2c.master, priv->i2c.addr, &reg, 1, true);
    i2c_read_blocking(priv->i2c.master, priv->i2c.addr, &val, 1, false);
    return val;
}
#define read_reg ft6236_read_reg

static void __ft6236_reset(struct ft6236_data *priv)
{
    gpio_put(priv->rst_pin, 1);
    sleep_ms(10);
    gpio_put(priv->rst_pin, 0);
    sleep_ms(10);
    gpio_put(priv->rst_pin, 1);
    sleep_ms(10);
}

static uint16_t __ft6236_read_x(struct ft6236_data *priv)
{
    uint8_t val_h = read_reg(priv, FT_REG_TOUCH1_XH) & 0x1f;  /* the MSB is always high, but it shouldn't */
    uint8_t val_l = read_reg(priv, FT_REG_TOUCH1_XL);
    uint16_t val = (val_h << 8) | val_l;
    
    if (priv->revert_x)
        return (priv->x_res - val);

    return val;
}

uint16_t ft6236_read_x(void)
{
    return g_ft6236_data.read_x(&g_ft6236_data);
}

static uint16_t __ft6236_read_y(struct ft6236_data *priv)
{
    uint8_t val_h = read_reg(priv, FT_REG_TOUCH1_YH);
    uint8_t val_l = read_reg(priv, FT_REG_TOUCH1_YL);
    if (priv->revert_y)
        return (priv->y_res - ((val_h << 8) | val_l));
    else
        return ((val_h << 8) | val_l);
}

uint16_t ft6236_read_y(void)
{
    return g_ft6236_data.read_y(&g_ft6236_data);
}

static bool __ft6236_is_pressed(struct ft6236_data *priv)
{
    uint8_t val = read_reg(priv, FT_REG_TD_STATUS);
    return val;
}

bool ft6236_is_pressed(void)
{
    return __ft6236_is_pressed(&g_ft6236_data);
}

static void __ft6236_set_dir(struct ft6236_data *priv, ft6236_direction_t dir)
{
    priv->dir = dir;

    if (dir & FT6236_DIR_REVERT_X)
        priv->revert_x = true;
    else
        priv->revert_x = false;

    if (dir & FT6236_DIR_REVERT_Y)
        priv->revert_y = true;
    else
        priv->revert_y = false;

    if (dir & FT6236_DIR_SWITCH_XY) {
        priv->read_x = __ft6236_read_y;
        priv->read_y = __ft6236_read_x;

        priv->revert_x = !priv->revert_x;
        priv->revert_y = !priv->revert_y;

        priv->x_res = FT6236_Y_RES;
        priv->y_res = FT6236_X_RES;
    } else {
        priv->read_x = __ft6236_read_x;
        priv->read_y = __ft6236_read_y;
    }
}

void ft6236_set_dir(ft6236_direction_t dir)
{
    __ft6236_set_dir(&g_ft6236_data, dir);
}

static void ft6236_hw_init(struct ft6236_data *priv)
{
    i2c_init(priv->i2c.master, FT6236_DEF_SPEED);

    gpio_set_function(priv->i2c.scl_pin, GPIO_FUNC_I2C);
    gpio_set_function(priv->i2c.sda_pin, GPIO_FUNC_I2C);

    gpio_pull_up(priv->i2c.scl_pin);
    gpio_pull_up(priv->i2c.sda_pin);

    gpio_init(priv->rst_pin);
    gpio_set_dir(priv->rst_pin, GPIO_OUT);
    gpio_pull_up(priv->rst_pin);

    __ft6236_reset(priv);

    /* registers are read-only */
    // write_reg(priv, FT_REG_DEVICE_MODE, 0x00);
    // write_reg(priv, FT_REG_TH_GROUP, 22);
    // write_reg(priv, FT_REG_PERIODACTIVE, 12);

    /* initialize touch direction */
    __ft6236_set_dir(priv, priv->dir);
}

static int ft6236_probe(struct ft6236_data *priv)
{
    priv->i2c.master  = i2c1;
    priv->i2c.addr    = FT6236_ADDR;
    priv->i2c.scl_pin = FT6236_PIN_SCL;
    priv->i2c.sda_pin = FT6236_PIN_SDA;

    priv->rst_pin     = FT6236_PIN_RST;

    priv->x_res = FT6236_X_RES;
    priv->y_res = FT6236_Y_RES;

    priv->revert_x = false;
    priv->revert_y = false;

    priv->dir = FT6236_DIR_SWITCH_XY | FT6236_DIR_REVERT_Y;

    ft6236_hw_init(priv);

    return 0;
}

int ft6236_driver_init(void)
{
    pr_debug("ft6236_driver_init\n");
    ft6236_probe(&g_ft6236_data);
    return 0;
}

STATIC mp_obj_t ft6236_init_func(void)
{
    ft6236_driver_init();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ft6236_init_obj, ft6236_init_func);

STATIC void mp_ft6236_ts_read(struct  _lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    static int32_t last_x = 0;
    static int32_t last_y = 0;

    if (ft6236_is_pressed()) {
        data->point.x = last_x = ft6236_read_x();
        data->point.y = last_y = ft6236_read_y();
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
DEFINE_PTR_OBJ(mp_ft6236_ts_read);

STATIC const mp_rom_map_elem_t ft6236_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ft6236) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&ft6236_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_ts_read), MP_ROM_PTR(&PTR_OBJ(mp_ft6236_ts_read)) },
};
STATIC MP_DEFINE_CONST_DICT(ft6236_module_globals, ft6236_module_globals_table);

const mp_obj_module_t ft6236_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&ft6236_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ft6236, ft6236_module);