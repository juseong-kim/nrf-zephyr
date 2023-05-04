#include "rms.h"

/* Logger */
LOG_MODULE_REGISTER(rms, LOG_LEVEL_DBG);

void add_v(int led, int val)
{
    int i0 = (led == 1 ? i1_1 : i1_2) % (N_VOLTAGE);
    int imin = led == 1 ? 0 : T_DATA_S;
    int imax = imin + T_DATA_S;

    // compute squared sums for each second
    for (int i = imin; i < imax; i++)
    {
        int i1 = (i0 + (i * N_VOLTAGE / T_DATA_S)) % (N_VOLTAGE);
        sqsum[i] -= led == 1 ? vq1[i1] * vq1[i1] : vq2[i1] * vq2[i1];
        if (i < imax - 1) {
            int i2 = (i0 + ((i + 1) * N_VOLTAGE / T_DATA_S)) % (N_VOLTAGE);
            sqsum[i] += led == 1 ? vq1[i2] * vq1[i2] : vq2[i2] * vq2[i2];
        }
        else {
            sqsum[i] += val * val;
            // LOG_DBG("sqsum[%d] = (%d)^2", i, val);
        }
    }

    // add new value (FIFO)
    if (led == 1){
        vq1[i0] = val;
        i1_1++;
        i1_1 %= (N_VOLTAGE);
    }
    else if (led == 2) {
        vq2[i0] = val;
        i1_2++;
        i1_2 %= (N_VOLTAGE);
    }

    calculate_rms();
}

void calculate_rms(void)
{
    // for (int i = 0; i < N_VOLTAGE; i++){
    //     int idx = (i1_1 + i) % (N_VOLTAGE);
    //     LOG_INF("vq1[%d] = %d", idx, vq1[idx]);
    // }
    // for (int i = 0; i < N_VOLTAGE; i++)
    // {
    //     int idx = (i1_2 + i) % (N_VOLTAGE);
    //     LOG_INF("vq2[%d] = %d", idx, vq2[idx]);
    // }
    for (int i = 0; i < N_BLE; i++)
    {
        LOG_DBG("ss[%d] = %f", i, sqsum[i]);
        LOG_DBG("vrms = %f \t=%d", sqrt(sqsum[i] / (N_VOLTAGE / T_DATA_S)), N_VOLTAGE / T_DATA_S);
        vble[i] = (uint16_t)sqrt(sqsum[i] / (N_VOLTAGE / T_DATA_S));
        // LOG_INF("vble[%d] = %d", i, vble[i]);
    }
}
