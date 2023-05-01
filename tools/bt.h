#ifndef BT_H
#define BT_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/services/bas.h>

#include "adc.h"

/* UUID of the Remote Service */
// Project ID: 0x0000 (3rd entry)
// MFG ID = 0x090e (4th entry)
#define BT_UUID_REMOTE_SERV_VAL \
    BT_UUID_128_ENCODE(0x8fcc2160, 0x4abd, 0x0000, 0x090e, 0x8e22d2fc7eb9)

/* UUID of the Data Characteristic */
#define BT_UUID_REMOTE_DATA_CHRC_VAL \
    BT_UUID_128_ENCODE(0x8fcc2161, 0x4abd, 0x0000, 0x090e, 0x8e22d2fc7eb9)

/* UUID of the Message Characteristic */
#define BT_UUID_REMOTE_MESSAGE_CHRC_VAL \
    BT_UUID_128_ENCODE(0x8fcc2162, 0x4abd, 0x0000, 0x090e, 0x8e22d2fc7eb9)

#define BT_UUID_REMOTE_SERVICE BT_UUID_DECLARE_128(BT_UUID_REMOTE_SERV_VAL)
#define BT_UUID_REMOTE_DATA_CHRC BT_UUID_DECLARE_128(BT_UUID_REMOTE_DATA_CHRC_VAL)
#define BT_UUID_REMOTE_MESSAGE_CHRC BT_UUID_DECLARE_128(BT_UUID_REMOTE_MESSAGE_CHRC_VAL)

enum bt_data_notifications_enabled
{
    BT_DATA_NOTIFICATIONS_ENABLED,
    BT_DATA_NOTIFICATIONS_DISABLED,
};

struct bt_remote_srv_cb
{
    void (*notif_changed)(enum bt_data_notifications_enabled status);
    void (*data_rx)(struct bt_conn *conn, const uint8_t *const data, uint16_t len);
};

/* Function declarations */
ssize_t read_data_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
void data_ccc_cfg_changed_cb(const struct bt_gatt_attr *attr, uint16_t value);
ssize_t on_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
void bluetooth_set_battery_level(int level, int nominal_batt_mv);
uint8_t bluetooth_get_battery_level(void);
void on_sent(struct bt_conn *conn, void *user_data);
void bt_ready(int ret);
int send_data_notification(struct bt_conn *conn, uint16_t length);
void set_data(uint8_t *data_in);
int bluetooth_init(struct bt_conn_cb *bt_cb, struct bt_remote_srv_cb *remote_cb);

/* Battery */
void check_battery_level(struct k_timer *timer);
uint8_t bluetooth_get_battery_level(void);
void bluetooth_set_battery_level(int level, int nominal_batt_level);

#endif