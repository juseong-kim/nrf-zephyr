#ifndef MACROS_H
#define MACROS_H

/* States */
#define STATE_DEFAULT 0
#define STATE_VBUS_DETECTED -1

/* ADC */
#define T_ADC_READ 500
#define T_ADC_READ_US 150
#define T_DATA 5000
#define T_DATA_S 5
#define N_VOLTAGE T_DATA * 1000 / T_ADC_READ_US
#define N_INPUT 2

/* Bluetooth */
#define N_BLE 10

/* Battery */
#define T_BAT_CHECK_S 5
#define T_BAT_CHECK ((T_BAT_CHECK_S * 1000) * (1000 / T_ADC_READ_US))
#define NOMINAL_BATT_MV 3700
#define BATT_FACTOR 0.48648649

/* VBUS */
#define T_VBUS_LED 500
#define T_VBUS 500

/* Input Voltages */
#define VPP_MIN1 5
#define VPP_MIN2 10
#define VPP_MAX1 50
#define VPP_MAX2 150

/* Miscellaneous */
#define nop

#endif