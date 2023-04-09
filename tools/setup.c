#include "setup.h"

/* Logger */
LOG_MODULE_REGISTER(setup, LOG_LEVEL_DBG);

void check_devices_ready(struct gpio_dt_spec led, struct adc_dt_spec adc, struct pwm_dt_spec pwm)
{
    // Check gpio0 interface
    if (!device_is_ready(led.port))
        LOG_ERR("GPIO0 not ready.");
    // Check ADC device
    if (!device_is_ready(adc.dev))
        LOG_ERR("ADC not ready.");
    // Check PWM
    if (!device_is_ready(pwm.dev))
        LOG_ERR("PWM not ready.");
}

void configure_pins(struct gpio_dt_spec led1, struct gpio_dt_spec led2,
                    struct gpio_dt_spec btn1, struct gpio_dt_spec btn2,
                    struct adc_dt_spec adc1, struct adc_dt_spec adc2)
{
    int err;
    // Output LED pins
    err = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_LOW);
    err += gpio_pin_configure_dt(&led2, GPIO_OUTPUT_LOW);
    if (err)
        LOG_ERR("Error configuring output LED pins.");

    // Input button pins
    err = gpio_pin_configure_dt(&btn1, GPIO_INPUT);
    err += gpio_pin_configure_dt(&btn2, GPIO_INPUT);
    if (err)
        LOG_ERR("Error configuring input button pins.");

    // ADC input pin
    err = adc_channel_setup_dt(&adc1);
    err += adc_channel_setup_dt(&adc2);
    if (err)
        LOG_ERR("Error configuring ADC input pins.");
}

/* Callbacks */
void on_save(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    extern int state;
    if (state == STATE_DEFAULT)
    {
        noop // save 5 data points for each input signal of RMS energy calculated each second
    }
}

void on_bt_send(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    extern int state;
    if (state == STATE_DEFAULT)
    {
        noop // send the two, 5-point data arrays to phone via Bluetooth
    }
}

void setup_callbacks(struct gpio_dt_spec btn1, struct gpio_dt_spec btn2)
{
    int err;
    /* Define variables of type static struct gpio_callback */
    static struct gpio_callback save_cb;
    static struct gpio_callback bt_cb;

    /* Configure interrupts on button pins */
    err = gpio_pin_interrupt_configure_dt(&btn1, GPIO_INT_EDGE_TO_ACTIVE);
    err += gpio_pin_interrupt_configure_dt(&btn2, GPIO_INT_EDGE_TO_ACTIVE);
    if (err) LOG_ERR("Error configuring pin interrupts.");

    /* Initialize gpio_callback variables */
    gpio_init_callback(&save_cb, on_save, BIT(btn1.pin));
    gpio_init_callback(&bt_cb, on_bt_send, BIT(btn2.pin));

    /* Attach callback functions */
    err = gpio_add_callback(btn1.port, &save_cb);
    err += gpio_add_callback(btn2.port, &bt_cb);
    if (err) LOG_ERR("Error attaching callback functions");
}