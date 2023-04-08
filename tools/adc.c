#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(adc, LOG_LEVEL_DBG);

/* ADC macros */
#define ADC_DT_SPEC_GET_BY_ALIAS(node_id){ 				\
	.dev = DEVICE_DT_GET(DT_PARENT(DT_ALIAS(node_id))),	\
	.channel_id = DT_REG_ADDR(DT_ALIAS(node_id)),		\
	ADC_CHANNEL_CFG_FROM_DT_NODE(DT_ALIAS(node_id))		\
} \

#define DT_SPEC_AND_COMMA(node_id, prop, idx)			\
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Functions */
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
		LOG_INF("%s (channel %d)\t%d mV", adc_channel.dev->name, adc_channel.channel_id, val_mv);
		return val_mv;
	}
}