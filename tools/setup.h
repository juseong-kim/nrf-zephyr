#ifndef SETUP_H
#define SETUP_H

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/pwm.h>
#include "macros.h"

/* States */
extern int state;
extern int err;
extern uint16_t vble[N_BLE];
extern float sqsum[N_BLE];
extern struct bt_conn *current_conn;

/* Functions */
void check_devices_ready(struct gpio_dt_spec led, struct pwm_dt_spec pwm,
                         struct adc_dt_spec adc0, struct adc_dt_spec adc1, struct adc_dt_spec adc2);
void configure_pins(struct gpio_dt_spec led1, struct gpio_dt_spec led2, struct gpio_dt_spec led3,
                    struct gpio_dt_spec btn1, struct gpio_dt_spec btn2,
                    struct adc_dt_spec adc0, struct adc_dt_spec adc1, struct adc_dt_spec adc2);
void on_save(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void on_bt_send(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void setup_callbacks(struct gpio_dt_spec btn1, struct gpio_dt_spec btn2);

#endif