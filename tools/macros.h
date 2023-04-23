#ifndef MACROS_H
#define MACROS_H

/* States */
#define STATE_DEFAULT 0
#define STATE_VBUS_DETECTED -1

#define T_ADC_READ 50
#define T_DATA 5000
#define T_DATA_S 5
#define N_VOLTAGE T_DATA / T_ADC_READ

#define N_BLE 10

#define noop

#endif