# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bme_mee)

target_sources(app PRIVATE src/main.c tools/setup.c tools/adc.c tools/rms.c tools/bt.c)
