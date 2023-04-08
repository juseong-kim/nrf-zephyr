#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

/* Register logger */
LOG_MODULE_REGISTER(lab07, LOG_LEVEL_DBG);

/* Function declarations */
int check_devices_ready(void);
int configure_pins(void);
void setup_callbacks(void);
void on_sleep(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void on_freq_up(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void on_freq_down(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void on_reset(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void set_state_LEDs(void);

/* No code statement */
#define noop

/* States */
#define STATE_DEFAULT 3
#define STATE_ERROR -1
#define STATE_SLEEP 4

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

/* Devicetree node identifiers for the LEDs. */
#define LED0_NODE DT_ALIAS(heartbeat)
#define LED1_NODE DT_ALIAS(buzzer)
#define LED2_NODE DT_ALIAS(ivdrip)
#define LED3_NODE DT_ALIAS(alarm)
#define LED4_NODE DT_ALIAS(error)
static const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec buzzer_led = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec ivdrip_led = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec alarm_led = GPIO_DT_SPEC_GET(LED3_NODE, gpios);
static const struct gpio_dt_spec error_led = GPIO_DT_SPEC_GET(LED4_NODE, gpios);

/* Devicetree node identifiers for the buttons. */
#define B0_NODE DT_ALIAS(button0)
#define B1_NODE DT_ALIAS(button1)
#define B2_NODE DT_ALIAS(button2)
#define B3_NODE DT_ALIAS(button3)
static const struct gpio_dt_spec sleep = GPIO_DT_SPEC_GET(B0_NODE, gpios);
static const struct gpio_dt_spec freq_up = GPIO_DT_SPEC_GET(B1_NODE, gpios);
static const struct gpio_dt_spec freq_down = GPIO_DT_SPEC_GET(B2_NODE, gpios);
static const struct gpio_dt_spec reset = GPIO_DT_SPEC_GET(B3_NODE, gpios);

/* Initialize variables */
static int delay = LED_ON_TIME_MS;
static int state = STATE_DEFAULT;
static int led_state = LED_STATE_B;

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
	int ret = 0;
	// Configure output LED pins
	ret = gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_LOW);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&ivdrip_led, GPIO_OUTPUT_LOW);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&alarm_led, GPIO_OUTPUT_LOW);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&buzzer_led, GPIO_OUTPUT_LOW);
	if (ret < 0) {
		return -1;
	}
	// Configure input button pins
	ret = gpio_pin_configure_dt(&sleep, GPIO_INPUT);
	if (ret < 0){
		return -1;
	}
	ret = gpio_pin_configure_dt(&freq_up, GPIO_INPUT);
	if (ret < 0){
		return -1;
	}
	ret = gpio_pin_configure_dt(&freq_down, GPIO_INPUT);
	if (ret < 0){
		return -1;
	}
	ret = gpio_pin_configure_dt(&reset, GPIO_INPUT);
	if (ret < 0){
		return -1;
	}
	return 0;
}

void setup_callbacks(void)
{
	/* Define variables of type static struct gpio_callback */
	static struct gpio_callback sleep_cb;
	static struct gpio_callback freq_up_cb;
	static struct gpio_callback freq_down_cb;
	static struct gpio_callback reset_cb;

	/* Configure interrupts on button pins */
	gpio_pin_interrupt_configure_dt(&sleep, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_pin_interrupt_configure_dt(&freq_up, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_pin_interrupt_configure_dt(&freq_down, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_pin_interrupt_configure_dt(&reset, GPIO_INT_EDGE_TO_ACTIVE);

	/* Initialize gpio_callback variables */
	gpio_init_callback(&sleep_cb, on_sleep, BIT(sleep.pin));
	gpio_init_callback(&freq_up_cb, on_freq_up, BIT(freq_up.pin));
	gpio_init_callback(&freq_down_cb, on_freq_down, BIT(freq_down.pin));
	gpio_init_callback(&reset_cb, on_reset, BIT(reset.pin));

	/* Attach callback functions */
	gpio_add_callback(sleep.port, &sleep_cb);
	gpio_add_callback(freq_up.port, &freq_up_cb);
	gpio_add_callback(freq_down.port, &freq_down_cb);
	gpio_add_callback(reset.port, &reset_cb);
}

void on_sleep(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	if (state == STATE_SLEEP) {
		// Wake and resume from state before sleep
		LOG_INF("Waking from sleep state");
		state = STATE_DEFAULT;
		led_state = (led_state - 1) % 3;
	}
	else if (state == STATE_DEFAULT) {
		// Sleep (turn all action LEDs off)
		LOG_INF("Entering sleep state");
		state = STATE_SLEEP;
		gpio_pin_set_dt(&buzzer_led, 0);
		gpio_pin_set_dt(&ivdrip_led, 0);
		gpio_pin_set_dt(&alarm_led, 0);
	}
	else { // Error state
		noop
	}
}

void on_freq_up(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (!(delay < MIN_ON_TIME_MS || delay > MAX_ON_TIME_MS) && state == STATE_DEFAULT){
		delay -= DEC_ON_TIME_MS;

		// If LED on-time decreases below 100 ms, set to error state
		if (delay < MIN_ON_TIME_MS) {
			state = STATE_ERROR;
			gpio_pin_set_dt(&error_led, 1);
			LOG_ERR("Maximum frequency reached. Press reset button to resume operation.");
		}
		else {
			LOG_INF("Action LED on-time = %d ms", delay);
		}
	}
	else if (state == STATE_ERROR) {
		noop
	}
}

void on_freq_down(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (!(delay < MIN_ON_TIME_MS || delay > MAX_ON_TIME_MS) && state == STATE_DEFAULT){
		delay += INC_ON_TIME_MS;

		// If LED on-time increases above 2000 ms, set to error state
		if (delay > MAX_ON_TIME_MS) {
			state = STATE_ERROR;
			gpio_pin_set_dt(&error_led, 1);
			LOG_ERR("Minimum frequency reached. Press reset button to resume operation.");
		}
		else {
			LOG_INF("Action LED on-time = %d ms", delay);
		}
	}
	else if (state == STATE_ERROR) {
		noop
	}
}

void on_reset(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	// Turn error LED off, set LED on-time to default 1000 ms
	// Return to default state if not in sleep state
	LOG_INF("Resetting action LED on-time to 1000 ms");
	gpio_pin_set_dt(&error_led, 0);
	delay = LED_ON_TIME_MS;
	state = state != STATE_SLEEP ? STATE_DEFAULT : STATE_SLEEP;
	LOG_INF("%s state", state != STATE_SLEEP ? "Returning to default" : "Staying in sleep");
}

void set_state_LEDs(void) {
	/* Set states of LEDs to the next state */
	gpio_pin_set_dt(&heartbeat_led, led_state == LED_STATE_A ? 1 : 0); // TODO set to independent 1 Hz, currently toggles with alarm_led

	gpio_pin_set_dt(&buzzer_led, led_state == LED_STATE_B ? 1 : 0);
	gpio_pin_set_dt(&ivdrip_led, led_state == LED_STATE_IV ? 1 : 0);
	gpio_pin_set_dt(&alarm_led, led_state == LED_STATE_A ? 1 : 0);
	led_state = (led_state + 1) % 3;
}

void main(void)
{
	// Check that devices, pins, and callbacks are ready and configured
	int err;
	err = check_devices_ready();
	if (err) {
		LOG_WRN("GPIO%d interface not ready.\n", err < -1 ? 0 : 1);
	}
	err = configure_pins();
	if (err) {
		LOG_WRN("Error configuring IO channels/pins.\n");
	}
	setup_callbacks();
	
	// Execute control logic
	while (1) {
		if (state == STATE_DEFAULT) {
			set_state_LEDs();
			k_msleep(delay);
		}
		else if (state == STATE_SLEEP ) {
			gpio_pin_toggle_dt(&heartbeat_led);
			k_msleep(1000);
		}
		else { // Error state
			k_msleep(100);
		}
	}
}