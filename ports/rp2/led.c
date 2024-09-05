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

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"

#include "hardware/gpio.h"

void hw_led_init(void)
{
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
}

void hw_led_on(void)
{
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
}

void hw_led_off(void)
{
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
}

STATIC mp_obj_t led_init_func(void)
{
    hw_led_init();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(led_init_obj, led_init_func);


STATIC mp_obj_t led_on_func(void)
{
    hw_led_on();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(led_on_obj, led_on_func);


STATIC mp_obj_t led_off_func(void)
{
    hw_led_off();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(led_off_obj, led_off_func);

STATIC const mp_rom_map_elem_t led_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),  MP_OBJ_NEW_QSTR(MP_QSTR_led) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&led_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_on), MP_ROM_PTR(&led_on_obj) },
    { MP_ROM_QSTR(MP_QSTR_off), MP_ROM_PTR(&led_off_obj) },
};
STATIC MP_DEFINE_CONST_DICT(led_module_globals, led_module_globals_table);

const mp_obj_module_t led_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&led_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_led, led_module);
