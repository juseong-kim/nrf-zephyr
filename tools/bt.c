#include "bt.h"
#include "macros.h"

LOG_MODULE_REGISTER(bt, LOG_LEVEL_INF);

static K_SEM_DEFINE(bt_init_ok, 1, 1);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
static uint16_t data[N_BLE] = {0};
static struct bt_remote_srv_cb remote_service_callbacks;
enum bt_data_notifications_enabled notifications_enabled;

/* Advertising data */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN)};

/* Scan response data */
static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_REMOTE_SERV_VAL),
};

/* Setup services */
BT_GATT_SERVICE_DEFINE(remote_srv,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_REMOTE_SERVICE),
                       BT_GATT_CHARACTERISTIC(BT_UUID_REMOTE_DATA_CHRC,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ,
                                              read_data_cb, NULL, NULL),
                       BT_GATT_CCC(data_ccc_cfg_changed_cb, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
                       BT_GATT_CHARACTERISTIC(BT_UUID_REMOTE_MESSAGE_CHRC,
                                              BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_WRITE,
                                              NULL, on_write, NULL), );

/* Callbacks */
void data_ccc_cfg_changed_cb(const struct bt_gatt_attr *attr, uint16_t value)
{
    bool notif_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Notifications: %s", notif_enabled ? "enabled" : "disabled");

    notifications_enabled = notif_enabled ? BT_DATA_NOTIFICATIONS_ENABLED : BT_DATA_NOTIFICATIONS_DISABLED;

    if (remote_service_callbacks.notif_changed)
    {
        remote_service_callbacks.notif_changed(notifications_enabled);
    }
}

ssize_t read_data_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &data, sizeof(data));
}

ssize_t on_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    LOG_INF("Received data, handle %d, conn %p", attr->handle, (void *)conn);

    if (remote_service_callbacks.data_rx)
    {
        remote_service_callbacks.data_rx(conn, buf, len);
    }
    return len;
}

void on_sent(struct bt_conn *conn, void *user_data)
{
    ARG_UNUSED(user_data);
    LOG_INF("Notification sent on connection %p", (void *)conn);
}

void bt_ready(int ret)
{
    if (ret) LOG_ERR("bt_enable returned %d", ret);
    // release thread once initialized
    k_sem_give(&bt_init_ok);
}

int send_data_notification(struct bt_conn *conn, uint16_t length)
{
    int ret = 0;

    struct bt_gatt_notify_params params = {0};
    const struct bt_gatt_attr *attr = &remote_srv.attrs[2];

    params.attr = attr;
    params.data = &data;
    params.len = length;
    params.func = on_sent;

    ret = bt_gatt_notify_cb(conn, &params);

    return ret;
}

void set_data(uint16_t *data_in)
{
    memcpy(data, data_in, sizeof(data));
    LOG_INF("Data set via memcpy (size = %d).", sizeof(data));
}

int bluetooth_init(struct bt_conn_cb *bt_cb, struct bt_remote_srv_cb *remote_cb)
{
    int ret;

    LOG_INF("Initializing Bluetooth");

    if ((bt_cb == NULL) | (remote_cb == NULL))
    {
        return -NRFX_ERROR_NULL;
    }
    bt_conn_cb_register(bt_cb);
    remote_service_callbacks.notif_changed = remote_cb->notif_changed;
    remote_service_callbacks.data_rx = remote_cb->data_rx;

    ret = bt_enable(bt_ready);
    if (ret)
    {
        LOG_ERR("bt_enable returned %d", ret);
        return ret;
    }

    // hold the thread until Bluetooth is initialized
    k_sem_take(&bt_init_ok, K_FOREVER);

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) settings_load();

    ret = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (ret)
    {
        LOG_ERR("Could not start advertising (ret = %d)", ret);
        return ret;
    }

    return ret;
}

struct bt_conn *current_conn;
void on_data_rx(struct bt_conn *conn, const uint16_t *const data, uint16_t len)
{
    // manually append NULL character at the end
    uint8_t temp_str[len + 1];
    memcpy(temp_str, data, len);
    temp_str[len] = 0x00;

    LOG_INF("BT received data on conn %p. Len: %d", (void *)conn, len);
    LOG_INF("Data: %s", temp_str);
}

void on_connected(struct bt_conn *conn, uint8_t ret)
{
    if (ret)
        LOG_ERR("Connection error: %d", ret);
    LOG_INF("BT connected");
    current_conn = bt_conn_ref(conn);
}

void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("BT disconnected (reason: %d)", reason);
    if (current_conn)
    {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
}

void on_notif_changed(enum bt_data_notifications_enabled status)
{
    if (status == BT_DATA_NOTIFICATIONS_ENABLED)
        LOG_INF("BT notifications enabled");
    else
        LOG_INF("BT notifications disabled");
}

/* Battery */
uint8_t bluetooth_get_battery_level(void){
    uint8_t battery_level;

    battery_level = bt_bas_get_battery_level();
    LOG_INF("Battery: %d%%", battery_level);
    
    return battery_level;
}

void bluetooth_set_battery_level(int level, int nominal_batt_level){
    LOG_DBG("Battery Voltage: %d", level);

    // Assume battery voltage is reduced by 0.48648649x via external hardware (i.e. 3.7V -> 1.8V)
    float normalized_level = (float)level * 100.0 / (nominal_batt_level * 0.48648649);
    if (normalized_level > 100) normalized_level = 100;
    else if (normalized_level < 0) normalized_level = 0;

    LOG_DBG("Battery Percentage: %.2f %%", normalized_level);

    int err = bt_bas_set_battery_level((uint8_t)normalized_level);
    if (err) LOG_ERR("BAS set error (err = %d)", err);
}

