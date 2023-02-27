# Lab 07: Zephyr Devicetree, GPIO & Callbacks
BME590L Medical Electrical Equipment  
Juseong (Joe) Kim

## Current status
Fully functional, except toggling of heartbeat LED at 1 Hz.  
All four buttons are prone to switch bouncing (i.e. single press is registered as two presses).

## Part 4
### What is the difference in configuring a GPIO LED pin to be GPIO ACTIVE LOW vs GPIO ACTIVE HIGH?
GPIO_ACTIVE_HIGH means that the LED is on when the pin is set to its high state, and off when the pin is in its low state.
### What is the purpose of calling device is ready() for the GPIO interfaces?
This function verifies that the given device is ready for use (i.e. in a state where it can be used with its standard API, successfully initalized). 
### What GPIO pins are used for button[0-3]?
GPIO pins P0.11, 0.12, 0.24, and 0.25 are used for buttons 0, 1, 2, and 3, respectively.
### Why do we use callbacks to detect the button presses (instead of simply reading the LOW/HIGH state of each button GPIO pin in main())?
Reading the state of each pin must be done continuously, resulting in excessive code overhead. The callbacks reduce the number of reads and optimize power efficiency by allowing for appropriate actions to take place only when the corresponding button is pressed (in this case, on the rising edge of the pin state, i.e. when the state changes to logical level 1).