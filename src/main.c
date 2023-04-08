#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>

#include <stdlib.h>

/* Logger */
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int vq[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // index via vq[(i1+i) % 10]
int i1 = 0;

void add_v(int q[10], int val){
	q[i1 % 10] = val;
	i1++;
	i1 %= 10;
}

void main(void)
{
	for (int i = 1; i < 16; i++)
	{
		add_v(vq, i);
		if (i>9){
			for (int i = 0; i < 10; ++i)
			{
				if (i < 9)
				{
					printf("%d, ", vq[(i1 + i) % 10]);
				}
				else
				{
					printf("%d\n", vq[(i1 + i) % 10]);
				}
			}
		}
	}
}