# Lab 07: Zephyr Devicetree, GPIO & Callbacks
BME590L Medical Electrical Equipment  
Juseong (Joe) Kim

## Remaining Questions
1. Why do the LEDs and buttons show no response?
1. How do you set the LED groups (1. heartbeat, 2. buzzer + ivdrip + alarm) to toggle at different frequencies? Is this achievable with k_msleep()? Is there a way to send the toggling of heartbeat_led to the background?

## Part 4
### What is the difference in configuring a GPIO LED pin to be GPIO ACTIVE LOW vs GPIO ACTIVE HIGH?
GPIO_ACTIVE_HIGH means that the LED is on when the pin is set to its high state, and off when the pin is in its low state.
### What is the purpose of calling device is ready() for the GPIO interfaces?
This function verifies that the given device is ready for use (i.e. in a state where it can be used with its standard API, successfully initalized). 
### What GPIO pins are used for button[0-3]?
GPIO pins P0.11, 0.12, 0.24, and 0.25 are used for buttons 0, 1, 2, and 3, respectively.
### Why do we use callbacks to detect the button presses (instead of simply reading the LOW/HIGH state of each button GPIO pin in main())?
To optimize for power efficiency, reducing computational overhead.