#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/usb_c/usbc_vbus.h>
#include <stdlib.h>

#include "../tools/setup.h"
#include "../tools/adc.h"

/* Logger */
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* States */
#define STATE_DEFAULT 0
#define STATE_VBUS_DETECTED -1
int state = STATE_DEFAULT;

/* Miscellaneous */
#define noop
int err;

/* ADC macros */
#define ADC_DT_SPEC_GET_BY_ALIAS(node_id)                   \
	{                                                       \
		.dev = DEVICE_DT_GET(DT_PARENT(DT_ALIAS(node_id))), \
		.channel_id = DT_REG_ADDR(DT_ALIAS(node_id)),       \
		ADC_CHANNEL_CFG_FROM_DT_NODE(DT_ALIAS(node_id))     \
	} \

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

#define PERIOD_ADC_READ 500

/* PWM */
const struct pwm_dt_spec pwm1 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_1));
const struct pwm_dt_spec pwm2 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_2));

/* LEDs */
const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led_1), gpios);
const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led_2), gpios);

/* Buttons */
const struct gpio_dt_spec btn_save = GPIO_DT_SPEC_GET(DT_ALIAS(btnsave), gpios);
const struct gpio_dt_spec btn_bt = GPIO_DT_SPEC_GET(DT_ALIAS(btnbt), gpios);

/* ADCs */
const struct adc_dt_spec adc1 = ADC_DT_SPEC_GET_BY_ALIAS(adc_1);
const struct adc_dt_spec adc2 = ADC_DT_SPEC_GET_BY_ALIAS(adc_2);

/* Voltages */
int vq[10] = {0}; // index via vq[(i1+i) % 10]
int i1 = 0;

void modulate_led_brightness(struct adc_dt_spec adc, struct pwm_dt_spec pwm){
	int mV = read_adc(adc);
	if (state == STATE_DEFAULT){
		// TODO update pulsewidth to RMS Voltage
		uint32_t pulsewidth = pwm.period * (mV / 3300.0); 
		LOG_INF("Pulse: %d", pulsewidth);
		err = pwm_set_pulse_dt(&pwm, pulsewidth);
		if(err) LOG_ERR("Error updating duty cycle of PWM channel %d", pwm.channel);
	}
	else {
		noop
	}
}

void main(void)
{
	check_devices_ready(led1, adc1, pwm1);
	configure_pins(led1, led2, btn_save, btn_bt, adc1, adc2);
	setup_callbacks(btn_save, btn_bt);
	while (1)
	{
		// if (!usbc_vbus_check_level(DT_ALIAS(usbd), 1)){
		// 	LOG_DBG("NOT above 1");
		// }
		k_msleep(PERIOD_ADC_READ);
		if (state == STATE_DEFAULT){
			modulate_led_brightness(adc1, pwm1);
			modulate_led_brightness(adc2, pwm2);
		}
	}
}