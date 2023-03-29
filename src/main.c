#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/pwm.h>

/* Register logger */
LOG_MODULE_REGISTER(ledControl, LOG_LEVEL_INF);

/* Function declarations */
int check_devices_ready(void);
int configure_pins(void);
int setup_callbacks(void);
void on_sleep(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void on_reset(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void toggle_heartbeat_led(struct k_timer *heartbeat_timer);
int read_adc(struct adc_dt_spec adc_channel);
void update_leds_freq(void);
void control_led2(struct k_timer *led2_timer);
void restart_timer(struct k_timer *timer, int duration_ms);

/* No code statement */
#define noop

/* States */
#define STATE_DEFAULT 0
#define STATE_ERROR -1
#define STATE_SLEEP 1

/* Time intervals */
#define HEARTBEAT_PERIOD 1000
#define ADC_READ_PERIOD 1000

/* ADC macros */
#define ADC_DT_SPEC_GET_BY_ALIAS(node_id){ 				\
	.dev = DEVICE_DT_GET(DT_PARENT(DT_ALIAS(node_id))),	\
	.channel_id = DT_REG_ADDR(DT_ALIAS(node_id)),		\
	ADC_CHANNEL_CFG_FROM_DT_NODE(DT_ALIAS(node_id))		\
} \

#define DT_SPEC_AND_COMMA(node_id, prop, idx)			\
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

#define mV_AIN_MIN 0
#define mV_AIN_MAX 3300
#define FREQ_MIN3 1
#define FREQ_MAX3 5
#define FREQ_MIN2 5
#define FREQ_MAX2 10

/* PWM */
static const struct pwm_dt_spec pwm_led2 = PWM_DT_SPEC_GET(DT_ALIAS(pwm2));
static const struct pwm_dt_spec pwm_led3 = PWM_DT_SPEC_GET(DT_ALIAS(pwm3));

/* LEDs */
static const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(heartbeat), gpios);
static const struct gpio_dt_spec led_2 = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct gpio_dt_spec led_3 = GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios);
static const struct gpio_dt_spec error_led = GPIO_DT_SPEC_GET(DT_ALIAS(error), gpios);

/* Buttons */
static const struct gpio_dt_spec sleep = GPIO_DT_SPEC_GET(DT_ALIAS(button0), gpios);
static const struct gpio_dt_spec freq_up = GPIO_DT_SPEC_GET(DT_ALIAS(button1), gpios);
static const struct gpio_dt_spec freq_down = GPIO_DT_SPEC_GET(DT_ALIAS(button2), gpios);
static const struct gpio_dt_spec reset = GPIO_DT_SPEC_GET(DT_ALIAS(button3), gpios);

/* LED Duty Cycle Steps */
static int cnt = 0;
static int duty_cycle[10] = {100, 80, 60, 40, 20, 0, 20, 40, 60, 80};

/* Timers */
K_TIMER_DEFINE(heartbeat_timer, toggle_heartbeat_led, NULL);
K_TIMER_DEFINE(led2_timer, control_led2, NULL);

/* Initialize LED structs with ADCs */
struct vModLED { // TODO update struct to include timer objects
	short int num;
	short int delay;
	float freq;
	short int freq_min;
	short int freq_max;
	const struct adc_dt_spec adc;
	struct k_timer *timer;
};
struct vModLED led2 = { 2, 500/FREQ_MIN2, FREQ_MIN2, FREQ_MIN2, FREQ_MAX2, ADC_DT_SPEC_GET_BY_ALIAS(vled2), &led2_timer };
struct vModLED led3 = { 3, 500/FREQ_MIN3, FREQ_MIN3, FREQ_MIN3, FREQ_MAX3, ADC_DT_SPEC_GET_BY_ALIAS(vled3) };
float voltage_to_freq(int mV, struct vModLED led);

/* Initialize state */
static int state = STATE_DEFAULT;

/* Function error code */
static int err;

int check_devices_ready(void)
{
	// Check gpio0 interface
	if (!device_is_ready(heartbeat_led.port)) return -1;
	// Check gpio1 interface
    if (!device_is_ready(error_led.port)) return -2;
	// Check ADC controller device
	if (!device_is_ready(led2.adc.dev)) return -3;
	// Check PWM
	if (!device_is_ready(pwm_led2.dev)) return -4;

	return 0;
}

int configure_pins(void)
{
	// Output LED pins
	err = gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_LOW);
	err += gpio_pin_configure_dt(&led_3, GPIO_OUTPUT_LOW);
	err += gpio_pin_configure_dt(&led_2, GPIO_OUTPUT_LOW);
	if (err) return -1;

	// Input button pins
	err = gpio_pin_configure_dt(&sleep, GPIO_INPUT);
	err += gpio_pin_configure_dt(&freq_up, GPIO_INPUT);
	err += gpio_pin_configure_dt(&freq_down, GPIO_INPUT);
	err += gpio_pin_configure_dt(&reset, GPIO_INPUT);
	if (err) return -2;

	// ADC input pin
	err = adc_channel_setup_dt(&(led2.adc));
	err +=  adc_channel_setup_dt(&(led3.adc));
	if (err) return -3;

	return 0;
}

int setup_callbacks(void)
{
	/* Define variables of type static struct gpio_callback */
	static struct gpio_callback sleep_cb;
	static struct gpio_callback reset_cb;

	/* Configure interrupts on button pins */
	err = gpio_pin_interrupt_configure_dt(&sleep, GPIO_INT_EDGE_TO_ACTIVE);
	err += gpio_pin_interrupt_configure_dt(&reset, GPIO_INT_EDGE_TO_ACTIVE);
	if (err) return -1;

	/* Initialize gpio_callback variables */
	gpio_init_callback(&sleep_cb, on_sleep, BIT(sleep.pin));
	gpio_init_callback(&reset_cb, on_reset, BIT(reset.pin));

	/* Attach callback functions */
	err = gpio_add_callback(sleep.port, &sleep_cb);
	err += gpio_add_callback(reset.port, &reset_cb);
	if (err) return -2;

	return 0;
}

/* Callbacks */
void on_sleep(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	if (state == STATE_SLEEP) {
		LOG_INF("Waking from sleep state");
		state = STATE_DEFAULT;
		// Resume with previous delay
		k_timer_start(&led2_timer, K_MSEC(led2.delay), K_MSEC(led2.delay));
	}
	else if (state == STATE_DEFAULT) {
		LOG_INF("Entering sleep state");
		state = STATE_SLEEP;
		// Sleep (turn all action LEDs off and stop timer to keep them off)
		err = pwm_set_pulse_dt(&pwm_led2, pwm_led2.period);
		err += pwm_set_pulse_dt(&pwm_led3, pwm_led3.period);
		if (err) {
			LOG_ERR("Error turning off all action LEDs.");
		}
		k_timer_stop(&led2_timer);
	}
	else { // Error state
		noop
	}
}

void on_reset(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_INF("Resetting device with minimum LED frequencies");
	gpio_pin_set_dt(&error_led, 0);

	state = STATE_DEFAULT;
	led2.delay = 100 / led2.freq_min;
	restart_timer(&led2_timer, led2.delay);
}

/* Timer functions */
void toggle_heartbeat_led(struct k_timer *heartbeat_timer){
	err = gpio_pin_toggle_dt(&heartbeat_led);
	if (err) LOG_ERR("Error toggling heartbeat LED.");
}

float voltage_to_freq(int mV, struct vModLED led) {
	mV = mV < 0 ? 0 : mV; // read negative voltages as 0 V
	float slope = 1.0 * (led.freq_max - led.freq_min) / (mV_AIN_MAX - mV_AIN_MIN);
	return led.freq_min + slope * (mV - mV_AIN_MIN);
}

int read_adc(struct adc_dt_spec adc_channel) {
	int16_t buf;
	int32_t val_mv;

	struct adc_sequence sequence = {
		.buffer = &buf,
		.buffer_size = sizeof(buf),
	};

	(void)adc_sequence_init_dt(&adc_channel, &sequence);

	err = adc_read(adc_channel.dev, &sequence);
	if (err < 0) LOG_ERR("Could not read(%d)", err);
	else LOG_DBG("Raw ADC Buffer: %d", buf);

	val_mv = buf;
	err = adc_raw_to_millivolts_dt(&adc_channel, &val_mv);
	if (err < 0) {
		LOG_ERR("Buffer cannot be converted to mV; returning raw buffer value.");
		return buf;
	}
	else {
		LOG_INF("%s (channel %d)\t%d mV", adc_channel.dev->name, adc_channel.channel_id, val_mv);
		return val_mv;
	}
}

void modulate_led_brightness(struct vModLED led){
	int mV = read_adc(led.adc);
	if (state == STATE_DEFAULT) {
		uint32_t pulsewidth = pwm_led3.period * (1 - mV / 3300.0);
		LOG_INF("LED%d   Pulse: %d ns = %.2f%%", led.num, pulsewidth, (1 - mV / 3300.0) * 100);
		err = pwm_set_pulse_dt(&pwm_led3, pulsewidth);
		if (err) LOG_ERR("Error updating LED3 duty cycle (PWM0).");
	}
	else {
		noop
	}
}

void modulate_led_frequency(struct vModLED led){
	int mV = read_adc(led.adc);
	led.freq = voltage_to_freq(mV, led);
	led.delay = (int)(100.0 / led.freq);
	if ((led.freq < led.freq_min || led.freq > led.freq_max) && state == STATE_DEFAULT) {
		LOG_ERR("LED%d frequency (%f Hz) out of range. Press reset button to resume operation.", led.num, led.freq);
		state = STATE_ERROR;
		err = gpio_pin_set_dt(&error_led, 1);
		if (err) LOG_ERR("Error setting error LED.");
		err = pwm_set_pulse_dt(&pwm_led2, 0);
		err += pwm_set_pulse_dt(&pwm_led3, 0);
		if (err) LOG_ERR("Error setting action LEDs.");
		k_timer_stop(led.timer);
	}
	else if (state == STATE_DEFAULT) {
		LOG_INF("LED%d   f = %f Hz\tdelay = %d ms", led.num, led.freq, led.delay);
		restart_timer(led.timer, led.delay);
	}
	else { // Do nothing in error or sleep state
		noop
	}
}

void control_led2(struct k_timer *led2_timer){
	// cycle through
	uint32_t pulsewidth = pwm_led2.period * (duty_cycle[cnt] / 100.0);
	err = pwm_set_pulse_dt(&pwm_led2, pulsewidth);
	LOG_DBG("LED2 Pulse Width = %d", pulsewidth);
	if (err) LOG_ERR("Error updating LED2 duty cycle (PWM0).");
	cnt += 1;
	if(cnt == 10) cnt = 0;
}

void restart_timer(struct k_timer *timer, int duration_ms){
	/* Stop current timer and restart with new duration */
	k_timer_stop(timer);
	k_timer_start(timer, K_MSEC(duration_ms), K_MSEC(duration_ms));
}

void main(void)
{
	/* Check that devices, pins, and callbacks are ready and configured */
	err = check_devices_ready();
	if (err) LOG_ERR("%s not ready.", err == -1 ? "GPIO0 interface" : err == -2 ? "GPIO1 interface" : err == -3 ? "ADC controller" : "PWM device"); // TODO consider adding dev name
	err = configure_pins();
	if (err) LOG_ERR("Error configuring %s pins.", err == -1 ? "output LED" : err == -2 ? "input button" : "ADC input");
	err = setup_callbacks();
	if (err) LOG_ERR("Error %s.", err < -1 ? "attaching callback functions" : "configuring pin interrupts");

	/* Set LED duty cycles */ 
	err = pwm_set_pulse_dt(&pwm_led2, pwm_led2.period/2);
	if (err) LOG_ERR("Error setting LED2 PWM (PWM0).");
	err = pwm_set_pulse_dt(&pwm_led3, pwm_led3.period/2);
	if (err) LOG_ERR("Error setting LED3 PWM (PWM1).");

	/* Start indefinite timers */
	k_timer_start(&heartbeat_timer, K_MSEC(HEARTBEAT_PERIOD/2), K_MSEC(HEARTBEAT_PERIOD/2));
	k_timer_start(&led2_timer, K_MSEC(led3.delay), K_MSEC(led3.delay));
	
	/* Read from ADC and update LEDs */
	while (1) {
		k_msleep(ADC_READ_PERIOD);
		if (state == STATE_DEFAULT) {
			LOG_INF("-------------");
			modulate_led_frequency(led2); // modulate LED2 brightness frequency with AIN1 voltage: sawtooth (optionally sinusoidal)
			modulate_led_brightness(led3); // modulate LED3 brightness amplitude (constantly ON) with AIN0 voltage: linear
		}
		else {
			noop
		}
	}
}