#!/bin/bash

make -C ports/rp2 BOARD=PICO submodules
make -j -C mpy-cross
make -j -C ports/rp2 BOARD=PICO USER_C_MODULES=../../lib/lv_bindings/bindings.cmake

ln -sf ports/rp2/build-PICO/compile_commands.json .
