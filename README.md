# Lab 07: Zephyr Devicetree, GPIO & Callbacks + Lab 08: Zephyr Timers + Lab 09: Zephyr ADC
BME590L Medical Electrical Equipment  
Juseong (Joe) Kim
Version: ADC

## Status
Voltage readings inaccurate.  
Buttons are prone to switch bouncing (i.e. single press is registered as two presses).  

## Remaining Questions
1. Can callback functions and timer functions return an error code (int)?
1. How to log float values?

## Lab 07 Questions
### What is the difference in configuring a GPIO LED pin to be GPIO ACTIVE LOW vs GPIO ACTIVE HIGH?
GPIO_ACTIVE_HIGH means that the LED is on when the pin is set to its high state, and off when the pin is in its low state.  
The converse is true for GPIO_ACTIVE_LOW (LED on if pin low, off if pin high).
### What is the purpose of calling device is ready() for the GPIO interfaces?
This function verifies that the given device is ready for use (i.e. in a state where it can be used with its standard API, successfully initalized). This must precede any code that uses such devices to ensure proper, expected operation.
### What GPIO pins are used for button[0-3]?
| Button | GPIO Pin |
| --- | --- |
| 0 | P0.11 |
| 1 | P0.12 |
| 2 | P0.24 |
| 3 | P0.25 |
### Why do we use callbacks to detect the button presses (instead of simply reading the LOW/HIGH state of each button GPIO pin in main())?
Reading the state of each pin must be done continuously, resulting in excessive code overhead. Tradeoffs can be made between the frequency of reads and the responsiveness of the program to button presses (i.e. changes in pin states). For example, if pin states are read every 1000 ms, in order to detect that the state of the pin is high, the button must be in the pressed state at the time of the read. Button presses (depress and release) occuring in the 1000 ms between reads will not be registered.    
The callbacks reduce the number/frequency of reads and optimize power efficiency by allowing for appropriate actions to take place only when the corresponding button is pressed (in this case, on the rising edge of the pin state, i.e. when the state changes to logical level 1). Button presses can occur at any time.