#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>

/* Register logger */
LOG_MODULE_REGISTER(ledControl, LOG_LEVEL_DBG);

/* Function declarations */
int check_devices_ready(void);
int configure_pins(void);
int setup_callbacks(void);
void on_sleep(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void on_reset(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void toggle_heartbeat_led(struct k_timer *heartbeat_timer);
int read_adc(struct adc_dt_spec adc_channel);
void update_leds_freq(struct k_timer *read_adc_timer);
void toggle_led2(struct k_timer *led2_timer);
void toggle_led3(struct k_timer *led3_timer);
void restart_timer(struct k_timer *timer, int duration_ms);

/* No code statement */
#define noop

/* States */
#define STATE_DEFAULT 0
#define STATE_ERROR -1
#define STATE_SLEEP 1

/* Time intervals */
#define HEARTBEAT_PERIOD 1000
#define ADC_READ_PERIOD 4000

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

/* LEDs */
static const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(heartbeat), gpios);
static const struct gpio_dt_spec buzzer_led = GPIO_DT_SPEC_GET(DT_ALIAS(buzzer), gpios);
static const struct gpio_dt_spec ivdrip_led = GPIO_DT_SPEC_GET(DT_ALIAS(ivdrip), gpios);
static const struct gpio_dt_spec error_led = GPIO_DT_SPEC_GET(DT_ALIAS(error), gpios);

/* Buttons */
static const struct gpio_dt_spec sleep = GPIO_DT_SPEC_GET(DT_ALIAS(button0), gpios);
static const struct gpio_dt_spec freq_up = GPIO_DT_SPEC_GET(DT_ALIAS(button1), gpios);
static const struct gpio_dt_spec freq_down = GPIO_DT_SPEC_GET(DT_ALIAS(button2), gpios);
static const struct gpio_dt_spec reset = GPIO_DT_SPEC_GET(DT_ALIAS(button3), gpios);

/* Initialize LED structs with ADCs */
struct vModLED {
	short int num;
	short int delay;
	float freq;
	short int freq_min;
	short int freq_max;
	const struct adc_dt_spec adc;
};
struct vModLED led2 = { 2, 1000/FREQ_MIN2, FREQ_MIN2, FREQ_MIN2, FREQ_MAX2, ADC_DT_SPEC_GET_BY_ALIAS(vled2) };
struct vModLED led3 = { 3, 1000/FREQ_MIN3, FREQ_MIN3, FREQ_MIN3, FREQ_MAX3, ADC_DT_SPEC_GET_BY_ALIAS(vled3) };
float voltage_to_freq(int mV, struct vModLED led);
void update_led_freq(struct vModLED led);

/* LED Frequencies */
float freq2, freq3;

/* Timers */
K_TIMER_DEFINE(heartbeat_timer, toggle_heartbeat_led, NULL);
K_TIMER_DEFINE(read_adc_timer, update_leds_freq, NULL);
K_TIMER_DEFINE(led2_timer, toggle_led2, NULL);
K_TIMER_DEFINE(led3_timer, toggle_led3, NULL);

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

	return 0;
}

int configure_pins(void)
{
	// Output LED pins
	err = gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_LOW);
	err += gpio_pin_configure_dt(&ivdrip_led, GPIO_OUTPUT_LOW);
	err += gpio_pin_configure_dt(&buzzer_led, GPIO_OUTPUT_LOW);
	if (err) return -1;

	// Input button pins
	err = gpio_pin_configure_dt(&sleep, GPIO_INPUT);
	err += gpio_pin_configure_dt(&freq_up, GPIO_INPUT);
	err += gpio_pin_configure_dt(&freq_down, GPIO_INPUT);
	err += gpio_pin_configure_dt(&reset, GPIO_INPUT);
	if (err) return -2;

	// ADC input pin
	err = adc_channel_setup_dt(&led2.adc);
	err +=  adc_channel_setup_dt(&led3.adc);
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
		k_timer_start(&read_adc_timer, K_MSEC(ADC_READ_PERIOD/2), K_MSEC(ADC_READ_PERIOD/2));
		k_timer_start(&led2_timer, K_MSEC(led2.delay), K_MSEC(led2.delay));
		k_timer_start(&led3_timer, K_MSEC(led3.delay), K_MSEC(led3.delay));
	}
	else if (state == STATE_DEFAULT) {
		LOG_INF("Entering sleep state");
		state = STATE_SLEEP;
		// Sleep (turn all action LEDs off and stop timer to keep them off)
		err = gpio_pin_set_dt(&buzzer_led, 0);
		err += gpio_pin_set_dt(&ivdrip_led, 0);
		if (err) {
			LOG_ERR("Error turning off all action LEDs.");
		}
		k_timer_stop(&read_adc_timer);
		k_timer_stop(&led2_timer);
		k_timer_stop(&led3_timer);
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
	led2.delay = 1000 / led2.freq_min;
	led3.delay= 1000 / led2.freq_min;

	restart_timer(&read_adc_timer, ADC_READ_PERIOD);
	restart_timer(&led2_timer, led2.delay);
	restart_timer(&led3_timer, led3.delay);
}

/* Timer functions */
void toggle_heartbeat_led(struct k_timer *heartbeat_timer){
	err = gpio_pin_toggle_dt(&heartbeat_led);
	if (err) LOG_ERR("Error toggling heartbeat LED.");
}

float voltage_to_freq(int mV, struct vModLED led) {
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

void update_led_freq(struct vModLED led){
	led.freq = voltage_to_freq(read_adc(led.adc), led);
	led.delay = (int)(1000.0 / led.freq);
	if ((led.freq < led.freq_min || led.freq > led.freq_max) && state == STATE_DEFAULT) {
		// TODO cannot log float values
		LOG_ERR("LED%d frequency (%d Hz) out of range. Press reset button to resume operation.", led.num, (int)led.freq);
		state = STATE_ERROR;
		err = gpio_pin_set_dt(&error_led, 1);
		if (err) LOG_ERR("Error setting error LED.");
		err = gpio_pin_set_dt(&buzzer_led, 1);
		err += gpio_pin_set_dt(&ivdrip_led, 1);
		if (err) LOG_ERR("Error setting action LEDs.");

		k_timer_stop(&read_adc_timer);
		if(led.num == 2) k_timer_stop(&led2_timer);
		if(led.num == 3) k_timer_stop(&led3_timer);
	}
	else if (state == STATE_DEFAULT) {
		// Set buzzer LED toggling to new frequency
		LOG_INF("LED%d\tf = %d Hz\t1/f = %d ms", led.num, (int)led.freq, led.delay);
		if(led.num == 2) restart_timer(&led2_timer, led.delay);
		if(led.num == 3) restart_timer(&led3_timer, led.delay);
	}
	else { // Do nothing in error or sleep state
		noop
	}
}

void update_leds_freq(struct k_timer *read_adc_timer){
	/* Update LED2 and LED3 frequencies */
	LOG_INF("------------------------");
	update_led_freq(led2);
	update_led_freq(led3);
}

void toggle_led2(struct k_timer *led2_timer){
	err = gpio_pin_toggle_dt(&buzzer_led);
	if (err) LOG_ERR("Error toggling buzzer LED.");
}

void toggle_led3(struct k_timer *led3_timer){
	err = gpio_pin_toggle_dt(&ivdrip_led);
	if (err) LOG_ERR("Error toggling IV drip LED.");
}

void restart_timer(struct k_timer *timer, int duration_ms){
	/* Stop current timer and restart with new duration */
	k_timer_stop(timer);
	k_timer_start(timer, K_MSEC(duration_ms), K_MSEC(duration_ms));
}

void main(void)
{
	// Check that devices, pins, and callbacks are ready and configured
	err = check_devices_ready();
	if (err) LOG_ERR("%s not ready.", err == -1 ? "GPIO0 interface" : err == -2 ? "GPIO1 interface" : "ADC controller");
	err = configure_pins();
	if (err) LOG_ERR("Error configuring %s pins.", err == -1 ? "output LED" : err == -2 ? "input button" : "ADC input");
	err = setup_callbacks();
	if (err) LOG_ERR("Error %s.", err < -1 ? "attaching callback functions" : "configuring pin interrupts");

	/* Start indefinite timers */
	k_timer_start(&heartbeat_timer, K_MSEC(HEARTBEAT_PERIOD/2), K_MSEC(HEARTBEAT_PERIOD/2));
	k_timer_start(&read_adc_timer, K_MSEC(ADC_READ_PERIOD), K_MSEC(ADC_READ_PERIOD));

	k_timer_start(&led2_timer, K_MSEC(led2.delay), K_MSEC(led2.delay));
	k_timer_start(&led3_timer, K_MSEC(led3.delay), K_MSEC(led3.delay));
	
	// Execute control logic indefinitely
	while (1) {
		k_msleep(10000);
	}
}