#include <zephyr/logging/log.h>
#include <stdint.h>
#include <math.h>
#include "macros.h"

/* for kprint */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

extern uint8_t vq1[N_VOLTAGE];
extern uint16_t vq2[N_VOLTAGE];
extern uint16_t vble[N_BLE];
extern int i1_1;
extern int i1_2;
extern float sqsum[N_BLE];

void add_v(int led, int val);
void calculate_rms(void);