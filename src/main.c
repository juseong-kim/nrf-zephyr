#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* States */
#define STATE_DEFAULT 0
#define STATE_ERROR 1

/* 1000 msec = 1 sec */
#define HEARTBEAT_PERIOD 1000
#define LED_ON_TIME_MS 1000
#define INC_ON_TIME_MS 100
#define DEC_ON_TIME_MS 100

/* The devicetree node identifier for the LEDs. */
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


/* The devicetree node identifier for the Buttons. */
#define B0_NODE DT_ALIAS(button0)
#define B1_NODE DT_ALIAS(button1)
#define B2_NODE DT_ALIAS(button2)
#define B3_NODE DT_ALIAS(button3)
static const struct gpio_dt_spec sleep = GPIO_DT_SPEC_GET(B0_NODE, gpios);
static const struct gpio_dt_spec freq_up = GPIO_DT_SPEC_GET(B1_NODE, gpios);
static const struct gpio_dt_spec freq_down = GPIO_DT_SPEC_GET(B2_NODE, gpios);
static const struct gpio_dt_spec reset = GPIO_DT_SPEC_GET(B3_NODE, gpios);

int delay = LED_ON_TIME_MS;
bool is_sleep = false, is_error = false;
bool heartbeat_prev, buzzer_prev, ivdrip_prev, alarm_prev, error_prev;

void ready(void)
{
	printk("Checking device is ready\n");
	if (!device_is_ready(heartbeat_led.port)) return;
    if (!device_is_ready(buzzer_led.port)) return;
    if (!device_is_ready(ivdrip_led.port)) return;
    if (!device_is_ready(alarm_led.port)) return;
    if (!device_is_ready(error_led.port)) return;
    if (!device_is_ready(sleep.port)) return;
    if (!device_is_ready(freq_up.port)) return;
    if (!device_is_ready(freq_down.port)) return;
	if (!device_is_ready(reset.port)) return;
}

void configure(void)
{
	printk("Configuring\n");
	int ret = 0;
	ret = gpio_pin_configure_dt(&sleep, GPIO_INPUT);
	if (ret < 0){
		return;
	}
	ret = gpio_pin_configure_dt(&freq_up, GPIO_INPUT);
	if (ret < 0){
		return;
	}
	ret = gpio_pin_configure_dt(&freq_down, GPIO_INPUT);
	if (ret < 0){
		return;
	}
	ret = gpio_pin_configure_dt(&reset, GPIO_INPUT);
	if (ret < 0){
		return;
	}
}

void on_sleep(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Sleep");
	is_sleep = !is_sleep;

	if (!is_sleep) {
		gpio_pin_set_dt(&heartbeat_led, heartbeat_prev);
		gpio_pin_set_dt(&buzzer_led, buzzer_prev);
		gpio_pin_set_dt(&ivdrip_led, ivdrip_prev);
		gpio_pin_set_dt(&alarm_led, alarm_prev);
	}
	else {
		// save previous state
		heartbeat_prev = gpio_pin_get_dt(&heartbeat_led);
		buzzer_prev = gpio_pin_get_dt(&buzzer_led);
		ivdrip_prev = gpio_pin_get_dt(&ivdrip_led);
		alarm_prev = gpio_pin_get_dt(&alarm_led);

		// turn off LEDs
		gpio_pin_set_dt(&heartbeat_led, 0);
		gpio_pin_set_dt(&buzzer_led, 0);
		gpio_pin_set_dt(&ivdrip_led, 0);
		gpio_pin_set_dt(&alarm_led, 0);
	}
}

void on_freq_up(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Freq up");
    if (!(delay < 100 || delay > 2000) && !is_error && !is_sleep){
		delay -= DEC_ON_TIME_MS;
		if (delay < 100) {
			is_error = true;
			gpio_pin_set_dt(&error_led, 1);
		}
	}
}

void on_freq_down(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Freq down");
    if (!(delay < 100 || delay > 2000) && !is_error && !is_sleep){
		delay += INC_ON_TIME_MS;
		if (delay > 2000) {
			is_error = true;
			gpio_pin_set_dt(&error_led, 1);
		}
	}
}

void on_reset(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Reset");
	is_error = false;
	gpio_pin_set_dt(&error_led, 1);
	is_sleep = false;
	delay = LED_ON_TIME_MS;
}

/* Define variables of type static struct gpio_callback */
static struct gpio_callback sleep_cb;
static struct gpio_callback freq_up_cb;
static struct gpio_callback freq_down_cb;
static struct gpio_callback reset_cb;

void main(void)
{
	ready();
	configure();
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

	int heartbeat_val;
	while (1) {
		printk("Beat");
		heartbeat_val = gpio_pin_toggle_dt(&heartbeat_led);
		if (heartbeat_val < 0) {
			return;
		}
		// TODO How to implement LEDs flashing at different intervals?
		
		k_msleep(HEARTBEAT_PERIOD);
	}
}
