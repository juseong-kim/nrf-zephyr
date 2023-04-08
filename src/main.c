#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>

#include <stdlib.h>

/* Logger */
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* K_FIFO Test */
// K_FIFO_DEFINE(fifo1);

// struct data_item_t {
// 	void *fifo_reserved;
// 	int val;
// };
// struct data_item_t tx1;
// struct data_item_t tx2;
// struct data_item_t *rx;

int vq[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
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

	/* K_FIFO Test */
	// for (int i=0;i<15;i++){
	// 	LOG_DBG("i=%d", i);
	// 	if(i==0){
	// 		tx1.val = 1;
	// 		k_fifo_put(&fifo1, &tx1);
	// 	}
	// 	else{
	// 		tx2.val = i;
	// 		k_fifo_put(&fifo1, &tx2);
	// 	}
	// 	k_msleep(100);

	// 	if (i > 3) {
	// 		rx = k_fifo_get(&fifo1, K_FOREVER);
	// 		LOG_DBG("val=%d", rx->val);
	// 	}
	// }
}