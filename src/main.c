#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

/* Register logger */
LOG_MODULE_REGISTER(ledControl, LOG_LEVEL_DBG);

/* Function declarations */
int check_devices_ready(void);
int configure_pins(void);
int setup_callbacks(void);
void on_sleep(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void on_freq_up(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void on_freq_down(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void on_reset(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void toggle_heartbeat_led(struct k_timer *heartbeat_timer);
void set_action_leds(struct k_timer *action_timer);

/* No code statement */
#define noop

/* States */
#define STATE_DEFAULT 0
#define STATE_ERROR -1
#define STATE_SLEEP 1

#define LED_STATE_B 0
#define LED_STATE_IV 1
#define LED_STATE_A 2

/* Time intervals */
#define HEARTBEAT_PERIOD 1000
#define LED_ON_TIME_MS 1000
#define INC_ON_TIME_MS 100
#define DEC_ON_TIME_MS 100
#define MAX_ON_TIME_MS 2000
#define MIN_ON_TIME_MS 100

/* LEDs */
static const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(heartbeat), gpios);
static const struct gpio_dt_spec buzzer_led = GPIO_DT_SPEC_GET(DT_ALIAS(buzzer), gpios);
static const struct gpio_dt_spec ivdrip_led = GPIO_DT_SPEC_GET(DT_ALIAS(ivdrip), gpios);
static const struct gpio_dt_spec alarm_led = GPIO_DT_SPEC_GET(DT_ALIAS(alarm), gpios);
static const struct gpio_dt_spec error_led = GPIO_DT_SPEC_GET(DT_ALIAS(error), gpios);

/* Buttons */
static const struct gpio_dt_spec sleep = GPIO_DT_SPEC_GET(DT_ALIAS(button0), gpios);
static const struct gpio_dt_spec freq_up = GPIO_DT_SPEC_GET(DT_ALIAS(button1), gpios);
static const struct gpio_dt_spec freq_down = GPIO_DT_SPEC_GET(DT_ALIAS(button2), gpios);
static const struct gpio_dt_spec reset = GPIO_DT_SPEC_GET(DT_ALIAS(button3), gpios);

/* Action LED Struct */
struct actionLEDs {
	short int state;
	short int delay;
};
struct actionLEDs aLEDs = { LED_STATE_B, LED_ON_TIME_MS };

/* Timers */
K_TIMER_DEFINE(heartbeat_timer, toggle_heartbeat_led, NULL);
K_TIMER_DEFINE(action_timer, set_action_leds, NULL);

/* Initialize state */
static int state = STATE_DEFAULT;

/* Function error code */
static int err;

int check_devices_ready(void)
{
	// Check gpio0 interface
	if (!device_is_ready(heartbeat_led.port)) return -2;
	// Check gpio1 interface
    if (!device_is_ready(error_led.port)) return -1;

	return 0;
}

int configure_pins(void)
{
	// Output LED pins
	err = gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_LOW);
	err += gpio_pin_configure_dt(&ivdrip_led, GPIO_OUTPUT_LOW);
	err += gpio_pin_configure_dt(&alarm_led, GPIO_OUTPUT_LOW);
	err += gpio_pin_configure_dt(&buzzer_led, GPIO_OUTPUT_LOW);
	if (err) return -1;

	// Input button pins
	err = gpio_pin_configure_dt(&sleep, GPIO_INPUT);
	err += gpio_pin_configure_dt(&freq_up, GPIO_INPUT);
	err += gpio_pin_configure_dt(&freq_down, GPIO_INPUT);
	err += gpio_pin_configure_dt(&reset, GPIO_INPUT);
	if (err) return -2;

	return 0;
}

int setup_callbacks(void)
{
	/* Define variables of type static struct gpio_callback */
	static struct gpio_callback sleep_cb;
	static struct gpio_callback freq_up_cb;
	static struct gpio_callback freq_down_cb;
	static struct gpio_callback reset_cb;

	/* Configure interrupts on button pins */
	err = gpio_pin_interrupt_configure_dt(&sleep, GPIO_INT_EDGE_TO_ACTIVE);
	err += gpio_pin_interrupt_configure_dt(&freq_up, GPIO_INT_EDGE_TO_ACTIVE);
	err += gpio_pin_interrupt_configure_dt(&freq_down, GPIO_INT_EDGE_TO_ACTIVE);
	err += gpio_pin_interrupt_configure_dt(&reset, GPIO_INT_EDGE_TO_ACTIVE);
	if (err) return -1;

	/* Initialize gpio_callback variables */
	gpio_init_callback(&sleep_cb, on_sleep, BIT(sleep.pin));
	gpio_init_callback(&freq_up_cb, on_freq_up, BIT(freq_up.pin));
	gpio_init_callback(&freq_down_cb, on_freq_down, BIT(freq_down.pin));
	gpio_init_callback(&reset_cb, on_reset, BIT(reset.pin));

	/* Attach callback functions */
	err = gpio_add_callback(sleep.port, &sleep_cb);
	err += gpio_add_callback(freq_up.port, &freq_up_cb);
	err += gpio_add_callback(freq_down.port, &freq_down_cb);
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
		// Get previous state
		aLEDs.state = aLEDs.state == LED_STATE_B ? LED_STATE_A : (aLEDs.state - 1) % 3;
		// Resume with previous delay
		k_timer_start(&action_timer, K_MSEC(aLEDs.delay), K_MSEC(aLEDs.delay));
	}
	else if (state == STATE_DEFAULT) {
		LOG_INF("Entering sleep state");
		state = STATE_SLEEP;
		// Sleep (turn all action LEDs off and stop timer to keep them off)
		err = gpio_pin_set_dt(&buzzer_led, 0);
		err += gpio_pin_set_dt(&ivdrip_led, 0);
		err += gpio_pin_set_dt(&alarm_led, 0);
		if (err) {
			LOG_ERR("Error turning off all action LEDs.");
		}
		k_timer_stop(&action_timer);
	}
	else { // Error state
		noop
	}
}

void on_freq_up(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (!(aLEDs.delay < MIN_ON_TIME_MS || aLEDs.delay > MAX_ON_TIME_MS) && state == STATE_DEFAULT){
		aLEDs.delay -= DEC_ON_TIME_MS;

		if (aLEDs.delay < MIN_ON_TIME_MS) {
			LOG_ERR("Maximum frequency reached. Press reset button to resume operation.");
			state = STATE_ERROR;
			err = gpio_pin_set_dt(&error_led, 1);
			if (err) LOG_ERR("Error setting error LED.");
			err = gpio_pin_set_dt(&buzzer_led, 1);
			err += gpio_pin_set_dt(&ivdrip_led, 1);
			err += gpio_pin_set_dt(&alarm_led, 1);
			if (err) LOG_ERR("Error setting action LEDs.");
			k_timer_stop(&action_timer);
		}
		else {
			// Set action LED toggling to new frequency
			LOG_INF("Action LED on-time = %d ms", aLEDs.delay);
			restart_timer(&action_timer, aLEDs.delay);
		}
	}
	else { // Do nothing in error or sleep state
		noop
	}
}

void on_freq_down(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (!(aLEDs.delay< MIN_ON_TIME_MS || aLEDs.delay > MAX_ON_TIME_MS) && state == STATE_DEFAULT){
		aLEDs.delay += INC_ON_TIME_MS;

		if (aLEDs.delay > MAX_ON_TIME_MS) {
			LOG_ERR("Minimum frequency reached. Press reset button to resume operation.");
			state = STATE_ERROR;
			err = gpio_pin_set_dt(&error_led, 1);
			if (err) LOG_ERR("Error setting error LED.");
			err = gpio_pin_set_dt(&buzzer_led, 1);
			err += gpio_pin_set_dt(&ivdrip_led, 1);
			err += gpio_pin_set_dt(&alarm_led, 1);
			if (err) LOG_ERR("Error setting action LEDs.");
			k_timer_stop(&action_timer);

		}
		else {
			// Set action LED toggling to new frequency
			LOG_INF("Action LED on-time = %d ms", aLEDs.delay);
			restart_timer(&action_timer, aLEDs.delay);
		}
	}
	else { // Do nothing in error or sleep state
		noop
	}
}

void on_reset(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_INF("Resetting device with default action LED on-time (1000 ms)");
	gpio_pin_set_dt(&error_led, 0);

	state = STATE_DEFAULT;
	aLEDs.delay= LED_ON_TIME_MS;
	restart_timer(&action_timer, aLEDs.delay);
}

void toggle_heartbeat_led(struct k_timer *heartbeat_timer){
	err = gpio_pin_toggle_dt(&heartbeat_led);
	if (err) LOG_ERR("Error toggling hearbeat LED");
}

void set_action_leds(struct k_timer *action_timer){
	/* Set states of LEDs to the next state */
	err = gpio_pin_set_dt(&buzzer_led, aLEDs.state == LED_STATE_B ? 1 : 0);
	err += gpio_pin_set_dt(&alarm_led, aLEDs.state == LED_STATE_A ? 1 : 0);
	err += gpio_pin_set_dt(&ivdrip_led, aLEDs.state == LED_STATE_IV ? 1 : 0);
	if (err) LOG_ERR("Error setting action LEDs to next state.");
	aLEDs.state = (aLEDs.state + 1) % 3;
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
	if (err) LOG_ERR("GPIO%d interface not ready.", err < -1 ? 0 : 1);
	err = configure_pins();
	if (err) LOG_ERR("Error configuring %s pins.", err < -1 ? "input button" : "output LED");
	err = setup_callbacks();
	if (err) LOG_ERR("Error %s.", err < -1 ? "attaching callback functions" : "configuring pin interrupts");

	/* Start indefinite timers */
	k_timer_start(&heartbeat_timer, K_MSEC(HEARTBEAT_PERIOD/2), K_MSEC(HEARTBEAT_PERIOD/2));
	k_timer_start(&action_timer, K_MSEC(aLEDs.delay), K_MSEC(aLEDs.delay));
	
	// Execute control logic indefinitely
	while (1) {
		k_msleep(10000);
	}
}