#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/usb_c/usbc_vbus.h>
#include <stdlib.h>

#include "../tools/setup.h"
#include "../tools/adc.h"
#include "../tools/bt.h"

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

/* BLE */
static struct bt_conn *current_conn;
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

/* Voltages */
static uint8_t vq[10] = {0}; // index via vq[(i1+i) % 10]
static int i1 = 0;

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
	err = bluetooth_init(&bluetooth_callbacks, &remote_service_callbacks);
	if (err) LOG_ERR("BT init failed (err = %d)", err);
	while (1)
	{
		// if (!usbc_vbus_check_level(DT_ALIAS(usbd), 1)){
		// 	LOG_DBG("NOT above 1");
		// }
		k_msleep(PERIOD_ADC_READ);
		if (state == STATE_DEFAULT){
			modulate_led_brightness(adc1, pwm1);
			modulate_led_brightness(adc2, pwm2);
			set_data(vq);
			err = send_data_notification(current_conn, vq, 1);
			if (err) LOG_ERR("Could not send BT notification (err: %d)", err);
			else LOG_INF("BT data transmitted.");
		}
	}
}