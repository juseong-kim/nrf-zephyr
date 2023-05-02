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
uint8_t vq2[N_VOLTAGE] = {0};
uint8_t vble[N_BLE] = {0};
int i1_1 = 0;
int i1_2 = 0;
long long int sqsum[N_BLE] = {0};

/* Miscellaneous */
int err;

/* Battery Level */
K_TIMER_DEFINE(battery_check_timer, check_battery_level, NULL);

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
K_TIMER_DEFINE(vbus_led_timer, toggle_vbus_led, NULL);

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

/* BLE */
struct bt_conn *current_conn;
void on_connected(struct bt_conn *conn, uint8_t ret);
void on_disconnected(struct bt_conn *conn, uint8_t reason);
void on_notif_changed(enum bt_data_notifications_enabled status);
void on_data_rx(struct bt_conn *conn, const uint8_t *const data, uint16_t len);
struct bt_conn_cb bluetooth_callbacks = {
	.connected = on_connected,
	.disconnected = on_disconnected,
};

struct bt_remote_srv_cb remote_service_callbacks = {
	.notif_changed = on_notif_changed,
	.data_rx = on_data_rx,
};

// BLE Callbacks
void on_data_rx(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
	// manually append NULL character at the end
	uint8_t temp_str[len + 1];
	memcpy(temp_str, data, len);
	temp_str[len] = 0x00;

	LOG_INF("BT received data on conn %p. Len: %d", (void *)conn, len);
	LOG_INF("Data: %s", temp_str);
}

void on_connected(struct bt_conn *conn, uint8_t ret)
{
	if (ret) LOG_ERR("Connection error: %d", ret);
	LOG_INF("BT connected");
	current_conn = bt_conn_ref(conn);
}

void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("BT disconnected (reason: %d)", reason);
	if (current_conn)
	{
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
}

void on_notif_changed(enum bt_data_notifications_enabled status)
{
	if (status == BT_DATA_NOTIFICATIONS_ENABLED) LOG_INF("BT notifications enabled");
	else LOG_INF("BT notifications disabled");
}

void modulate_led_brightness(struct adc_dt_spec adc, struct pwm_dt_spec pwm, int led)
{
	int mV = read_adc(adc);
	if (state == STATE_DEFAULT){
		add_v(led, mV);
		uint32_t pulsewidth = pwm.period * (vble[T_DATA_S-1] / 3300.0);
		// LOG_DBG("LED%d RMS = %d", led, vble[T_DATA_S - 1]);
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
	// int limit = 0;
	k_timer_start(&battery_check_timer, K_SECONDS(T_BAT_CHECK_S), K_SECONDS(T_BAT_CHECK_S));
	k_timer_start(&vbus_timer, K_SECONDS(T_VBUS_S), K_SECONDS(T_VBUS_S));

	while (1)
	{
		// if (!usbc_vbus_check_level(DT_ALIAS(usbd), 1)){
		// 	LOG_DBG("NOT above 1");
		// }
		k_msleep(T_ADC_READ);
		if (state == STATE_DEFAULT){
			// TODO: implement below in timer
			modulate_led_brightness(adc1, pwm1, 1);
			modulate_led_brightness(adc2, pwm2, 2);
		}
		// limit++;
	}
}