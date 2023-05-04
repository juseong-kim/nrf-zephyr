#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
// #include <zephyr/drivers/usb_c/usbc_vbus.h>
#include <stdlib.h>
#include <nrfx_power.h>

#include "../tools/macros.h"
#include "../tools/setup.h"
#include "../tools/adc.h"
#include "../tools/bt.h"
#include "../tools/rms.h"

/* Logger */
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* States */
int state = STATE_DEFAULT;

/* Voltages */
uint8_t vq1[N_VOLTAGE] = {0}; // index via vble[(i1+i) % 10]
uint16_t vq2[N_VOLTAGE] = {0};
uint16_t vble[N_BLE] = {0};
int i1_1 = 0;
int i1_2 = 0;
float sqsum[N_BLE] = {0};

/* Miscellaneous */
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

/* PWM */
const struct pwm_dt_spec pwm1 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_1));
const struct pwm_dt_spec pwm2 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_2));

/* LEDs */
const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led_1), gpios);
const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led_2), gpios);
const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(DT_ALIAS(led_3), gpios);

/* Buttons */
const struct gpio_dt_spec btn_save = GPIO_DT_SPEC_GET(DT_ALIAS(btnsave), gpios);
const struct gpio_dt_spec btn_bt = GPIO_DT_SPEC_GET(DT_ALIAS(btnbt), gpios);

/* ADCs */
const struct adc_dt_spec adc1 = ADC_DT_SPEC_GET_BY_ALIAS(adc_1);
const struct adc_dt_spec adc2 = ADC_DT_SPEC_GET_BY_ALIAS(adc_2);
const struct adc_dt_spec adc_bat = ADC_DT_SPEC_GET_BY_ALIAS(adc_3);

/* VBUS */
void toggle_vbus_led(struct k_timer *vbus_led_timer);
void toggle_vbus_led(struct k_timer *vbus_led_timer)
{
	err = gpio_pin_toggle_dt(&led3);
	if (err)
		LOG_ERR("Error toggling VBUS LED.");
}
void set_vbus_led_off(struct k_timer *vbus_led_timer);
void set_vbus_led_off(struct k_timer *vbus_led_timer)
{
	err = gpio_pin_set_dt(&led3, 0);
	if (err)
		LOG_ERR("Error turning off VBUS LED.");
}
K_TIMER_DEFINE(vbus_led_timer, toggle_vbus_led, set_vbus_led_off);

void check_vbus(struct k_timer *vbus_timer);
void check_vbus(struct k_timer *vbus_timer) {
	bool usbregstatus = nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
	if (usbregstatus) {
		LOG_ERR("VBUS voltage detected. Device cannot be operated while charging.");
		state = STATE_VBUS_DETECTED;
		if(k_timer_remaining_get(&vbus_led_timer) == 0)
			k_timer_start(&vbus_led_timer, K_MSEC(T_VBUS_LED), K_MSEC(T_VBUS_LED));
	}
	else {
		LOG_DBG("VBUS voltage checked and not detected.");
		k_timer_stop(&vbus_led_timer);
		state = STATE_DEFAULT;
	}
}
K_TIMER_DEFINE(vbus_timer, check_vbus, NULL);

/* Battery Level */
void check_battery_level(struct k_timer *timer);
void check_battery_level(struct k_timer *timer)
{
	// Bluetooth set battery level
	int mV = read_adc(adc_bat);
	bluetooth_set_battery_level(mV, NOMINAL_BATT_MV);
	// uint8_t battery_level = bluetooth_get_battery_level();
	// set battery indicators (LEDs)
}
K_TIMER_DEFINE(battery_check_timer, check_battery_level, NULL);

/* BLE */
extern struct bt_conn *current_conn;
struct bt_conn_cb bluetooth_callbacks = {
	.connected = on_connected,
	.disconnected = on_disconnected,
};

struct bt_remote_srv_cb remote_service_callbacks = {
	.notif_changed = on_notif_changed,
	.data_rx = on_data_rx,
};

void modulate_led_brightness(struct adc_dt_spec adc, struct pwm_dt_spec pwm, int led)
{
	int mV = read_adc(adc);
	if (state == STATE_DEFAULT){
		add_v(led, mV);
		int vpp_max = led == 1 ? VPP_MAX1 : VPP_MAX2;
		int vble_idx = led == 1 ? T_DATA_S - 1 : (T_DATA_S * 2) - 1;
		double pwm_frac = vble[vble_idx] < vpp_max ? vble[vble_idx] / (double)vpp_max : 0.99;
		uint32_t pulsewidth = pwm.period * pwm_frac;
		LOG_INF("LED%d PW = %d = \t%d * %f\t(%d / %d)", led, pulsewidth, pwm.period, pwm_frac, vble[vble_idx], vpp_max);
		err = pwm_set_pulse_dt(&pwm, pulsewidth);
		if(err) LOG_ERR("Error updating duty cycle of PWM channel %d", pwm.channel);
	}
	else {
		nop
	}
}

void main(void)
{
	check_devices_ready(led1, pwm1, adc1, adc2, adc_bat);
	configure_pins(led1, led2, led3, btn_save, btn_bt, adc1, adc2, adc_bat);
	setup_callbacks(btn_save, btn_bt);
	err = bluetooth_init(&bluetooth_callbacks, &remote_service_callbacks);
	if (err) LOG_ERR("BT init failed (err = %d)", err);
	// k_timer_start(&battery_check_timer, K_SECONDS(T_BAT_CHECK_S), K_SECONDS(T_BAT_CHECK_S));
	k_timer_start(&vbus_timer, K_SECONDS(T_VBUS_S), K_SECONDS(T_VBUS_S));
	while (1)
	{
		k_msleep(T_ADC_READ);
		if (state == STATE_DEFAULT)
		{
			modulate_led_brightness(adc1, pwm1, 1);
			modulate_led_brightness(adc2, pwm2, 2);
		}
	}
}