#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(adc, LOG_LEVEL_INF);

int read_adc(struct adc_dt_spec adc_channel) {
	int16_t buf;
	int32_t val_mv;

	struct adc_sequence sequence = {
		.buffer = &buf,
		.buffer_size = sizeof(buf),
	};

	(void)adc_sequence_init_dt(&adc_channel, &sequence);

	int err = adc_read(adc_channel.dev, &sequence);
	if (err < 0) LOG_ERR("Could not read(%d)", err);
	else LOG_DBG("Raw ADC Buffer: %d", buf);

	val_mv = buf;
	err = adc_raw_to_millivolts_dt(&adc_channel, &val_mv);
	if (err < 0) {
		LOG_ERR("Buffer cannot be converted to mV; returning raw buffer value.");
		return buf;
	}
	else {
		LOG_DBG("%s (channel %d)\t%d mV", adc_channel.dev->name, adc_channel.channel_id, val_mv);
		return val_mv;
	}
}