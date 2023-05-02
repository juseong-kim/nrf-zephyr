#include "setup.h"
#include "bt.h"
#include "rms.h"

/* Logger */
LOG_MODULE_REGISTER(setup, LOG_LEVEL_DBG);

void check_devices_ready(struct gpio_dt_spec led, struct pwm_dt_spec pwm,
                         struct adc_dt_spec adc0, struct adc_dt_spec adc1, struct adc_dt_spec adc2)
{
    // Check gpio0 interface
    if (!device_is_ready(led.port))
        LOG_ERR("GPIO0 not ready.");
    // Check ADC devices
    if (!device_is_ready(adc0.dev))
        LOG_ERR("ADC Channel 0 not ready.");
    if (!device_is_ready(adc1.dev))
        LOG_ERR("ADC Channel 1 not ready.");
    if (!device_is_ready(adc2.dev))
        LOG_ERR("ADC Channel 2 not ready.");
    // Check PWM
    if (!device_is_ready(pwm.dev))
        LOG_ERR("PWM not ready.");
}

void configure_pins(struct gpio_dt_spec led1, struct gpio_dt_spec led2, struct gpio_dt_spec led3,
                    struct gpio_dt_spec btn1, struct gpio_dt_spec btn2,
                    struct adc_dt_spec adc0, struct adc_dt_spec adc1, struct adc_dt_spec adc2)
{
    int err;
    // Output LED pins
    err = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_LOW);
    err = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_LOW);
    err += gpio_pin_configure_dt(&led3, GPIO_OUTPUT_LOW);
    if (err)
        LOG_ERR("Error configuring output LED pins.");

    // Input button pins
    err = gpio_pin_configure_dt(&btn1, GPIO_INPUT);
    err += gpio_pin_configure_dt(&btn2, GPIO_INPUT);
    if (err)
        LOG_ERR("Error configuring input button pins.");

    // ADC input pin
    err = adc_channel_setup_dt(&adc0);
    err += adc_channel_setup_dt(&adc1);
    err += adc_channel_setup_dt(&adc2);
    if (err)
        LOG_ERR("Error configuring ADC input pins.");
}

/* Callbacks */
void on_save(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (state == STATE_DEFAULT)
    {
        calculate_rms();
        set_data(vble);
        // set_data(SAMPLE_DATA);
    }
}

void on_bt_send(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (state == STATE_DEFAULT)
    {
        // int idx; // send the two, 5-point data arrays to phone via Bluetooth
        LOG_INF("Sending arrays to phone via Bluetooth");

        err = send_data_notification(current_conn, N_BLE);
        if (err)
            LOG_ERR("Could not send BT notification (err: %d)", err);
    }
}

void setup_callbacks(struct gpio_dt_spec btn1, struct gpio_dt_spec btn2)
{
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