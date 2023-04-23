#include "rms.h"

/* Logger */
LOG_MODULE_REGISTER(rms, LOG_LEVEL_DBG);

void add_v(int led, int val)
{
    val = val < 0 ? 0 : val;

    // compute squared sums for each second
    int i0 = i1_1 % (N_VOLTAGE);
    int imin = led == 1 ? 0 : T_DATA_S;
    int imax = imin + T_DATA_S;
    for (int i = imin; i < imax; i++)
    {
        int i1 = (i0 + (i * N_VOLTAGE / T_DATA_S)) % (N_VOLTAGE);
        sqsum[i] -= led == 1 ? vq1[i1] * vq1[i1] : vq2[i1] * vq2[i1];
        // printk("\nss[%d] -= %d", i, vq1[i1]*vq1[i1]);
        if (i < imax - 1) {
            int i2 = (i0 + ((i + 1) * N_VOLTAGE / T_DATA_S)) % (N_VOLTAGE);
            sqsum[i] += led == 1 ? vq1[i2] * vq1[i2] : vq2[i2] * vq2[i2];
            // if (led == 1) printk("\nss[%d] += vq1[%d]^2 = %d", i, i2, vq1[i2] * vq1[i2]);
        }
        else {
            sqsum[i] += val * val;
            // if (led == 1) printk("\nss[%d] += %d", i, val * val);
        }
    }

    // add new value (FIFO)
    if (led == 1){
        vq1[i1_1 % (N_VOLTAGE)] = val;
        i1_1++;
        i1_1 %= (N_VOLTAGE);
    }
    else if (led == 2) {
        vq2[i1_2 % (N_VOLTAGE)] = val;
        i1_2++;
        i1_2 %= (N_VOLTAGE);
    }

    // printk("\nvq1: ");
    // for (int i = 0; i < N_VOLTAGE; i++)
    // {
    //     int idx = (i1_1 + i) % (N_VOLTAGE);
    //     if (idx == 0) {
    //         printk("*%d,", vq1[idx]);
    //     }
    //     else {
    //         printk("%d,", vq1[idx]);
    //     }
    // }
    // printk("\nss1: ");
    // for (int i = 0; i < N_BLE/2; i++)
    // {
    //     if (i == 0)
    //     {
    //         printk("*%d,", sqsum[i]);
    //     }
    //     else
    //     {
    //         printk("%d,", sqsum[i]);
    //     }
    // }
}

void calculate_rms(void)
{
    for (int i = 0; i < N_VOLTAGE; i++){
        int idx = (i1_1 + i) % (N_VOLTAGE);
        LOG_DBG("vq1[%d] = %d", idx, vq1[idx]);
    }
    for (int i = 0; i < N_BLE; i++)
    {
        LOG_DBG("ss[%d] = %lld", i, sqsum[i]);
        vble[i] = (int)sqrt(sqsum[i] / (N_VOLTAGE / T_DATA_S));
    }
}
