#include "cyd_pair.h"
#include "cyd_pair_ble.h"
#include <stdio.h>
#include <string.h>
#if CYD_PAIR_MANAGER && defined(CONFIG_BT_NIMBLE_ENABLED)
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "store/config/ble_store_config.h"
void ble_store_config_init(void);
#include "services/gap/ble_svc_gap.h"
static SemaphoreHandle_t guard;
static bool started, synced, scanning, connecting;
static uint8_t own_type;
static uint16_t connection = BLE_HS_CONN_HANDLE_NONE;
static struct {
    ble_addr_t address;
    char name[33];
    int rssi;
} found[16];
static unsigned count;
static char message[128] = "Bluetooth LE is off. Scan to enable it.", peer_name[33];
static void note(const char *s) {
    xSemaphoreTake(guard, portMAX_DELAY);
    snprintf(message, sizeof(message), "%s", s);
    xSemaphoreGive(guard);
    cyd_log("bluetooth", s);
}
static int gap(struct ble_gap_event *event, void *arg) {
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        struct ble_hs_adv_fields fields;
        if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data))
            return 0;
        xSemaphoreTake(guard, portMAX_DELAY);
        unsigned i;
        for (i = 0; i < count; i++)
            if (!memcmp(&found[i].address, &event->disc.addr, sizeof(ble_addr_t)))
                break;
        if (i < count || count < 16) {
            if (i == count)
                count++;
            found[i].address = event->disc.addr;
            found[i].rssi = event->disc.rssi;
            if (fields.name && fields.name_len) {
                unsigned n = fields.name_len;
                if (n > 32)
                    n = 32;
                memcpy(found[i].name, fields.name, n);
                found[i].name[n] = 0;
                for (unsigned j = 0; j < n; j++)
                    if ((unsigned char)found[i].name[j] < 32)
                        found[i].name[j] = '?';
            }
        }
        xSemaphoreGive(guard);
        break;
    }
    case BLE_GAP_EVENT_DISC_COMPLETE:
        xSemaphoreTake(guard, portMAX_DELAY);
        scanning = false;
        xSemaphoreGive(guard);
        note("Scan complete; select a device to connect");
        break;
    case BLE_GAP_EVENT_CONNECT:
        xSemaphoreTake(guard, portMAX_DELAY);
        connecting = false;
        connection = event->connect.status ? BLE_HS_CONN_HANDLE_NONE : event->connect.conn_handle;
        xSemaphoreGive(guard);
        note(event->connect.status ? "Connection failed" : "BLE link connected");
        if (!event->connect.status)
            ble_gap_security_initiate(event->connect.conn_handle);
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        xSemaphoreTake(guard, portMAX_DELAY);
        connection = BLE_HS_CONN_HANDLE_NONE;
        connecting = false;
        xSemaphoreGive(guard);
        note("Disconnected");
        break;
    case BLE_GAP_EVENT_ENC_CHANGE:
        note(event->enc_change.status ? "Connected; encryption unavailable for this peripheral"
                                      : "Connected; link encrypted");
        break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        note("This peripheral needs a passkey flow; connection closed");
        ble_gap_terminate(event->passkey.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* Do not silently erase an existing bond to accept a different identity. */
        note("Existing bond differs; re-pairing was rejected");
        return BLE_GAP_REPEAT_PAIRING_IGNORE;
    default:
        break;
    }
    return 0;
}
static void sync_host(void) {
    if (!ble_hs_id_infer_auto(0, &own_type)) {
        xSemaphoreTake(guard, portMAX_DELAY);
        synced = true;
        xSemaphoreGive(guard);
        note("Bluetooth LE ready; press Scan");
    }
}
static void reset_host(int reason) {
    (void)reason;
    xSemaphoreTake(guard, portMAX_DELAY);
    synced = false;
    connection = BLE_HS_CONN_HANDLE_NONE;
    xSemaphoreGive(guard);
    note("Bluetooth controller reset; waiting for synchronization");
}
static void host_task(void *arg) {
    (void)arg;
    nimble_port_run();
    nimble_port_freertos_deinit();
}
static esp_err_t enable(void) {
    if (started)
        return ESP_OK;
    if (!guard)
        guard = xSemaphoreCreateMutex();
    if (!guard)
        return ESP_ERR_NO_MEM;
    esp_err_t e = nimble_port_init();
    if (e) {
        note("Bluetooth memory/controller unavailable; Wi-Fi remains active");
        return e;
    }
    ble_hs_cfg.sync_cb = sync_host;
    ble_hs_cfg.reset_cb = reset_host;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_store_config_init();

    started = true;
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}
esp_err_t cyd_pair_ble_scan(void) {
    esp_err_t e = enable();
    if (e)
        return e;
    xSemaphoreTake(guard, portMAX_DELAY);
    bool available = synced && !scanning && !connecting;
    bool linked = connection != BLE_HS_CONN_HANDLE_NONE;
    if (available && !linked) {
        count = 0;
        memset(found, 0, sizeof(found));
        scanning = true;
    }
    xSemaphoreGive(guard);
    if (!available || linked)
        return ESP_ERR_INVALID_STATE;
    struct ble_gap_disc_params params = {
        .passive = 1, .itvl = 0x80, .window = 0x30, .filter_duplicates = 1};
    int rc = ble_gap_disc(own_type, 6000, &params, gap, NULL);
    if (rc) {
        xSemaphoreTake(guard, portMAX_DELAY);
        scanning = false;
        xSemaphoreGive(guard);
        return ESP_FAIL;
    }
    note("Scanning for Bluetooth LE devices (6 seconds)");
    return ESP_OK;
}
esp_err_t cyd_pair_ble_connect(unsigned index) {
    if (!guard)
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(guard, portMAX_DELAY);
    if (!synced || scanning || connecting || connection != BLE_HS_CONN_HANDLE_NONE ||
        index >= count) {
        xSemaphoreGive(guard);
        return ESP_ERR_INVALID_STATE;
    }
    ble_addr_t address = found[index].address;
    snprintf(peer_name, sizeof(peer_name), "%s", found[index].name);
    connecting = true;
    xSemaphoreGive(guard);
    int rc = ble_gap_connect(own_type, &address, 10000, NULL, gap, NULL);
    if (rc) {
        xSemaphoreTake(guard, portMAX_DELAY);
        connecting = false;
        xSemaphoreGive(guard);
        return ESP_FAIL;
    }
    note("Connecting to the selected BLE device");
    return ESP_OK;
}
esp_err_t cyd_pair_ble_disconnect(void) {
    if (!guard)
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(guard, portMAX_DELAY);
    uint16_t handle = connection;
    bool pending = connecting;
    xSemaphoreGive(guard);
    if (pending)
        return ble_gap_conn_cancel() ? ESP_FAIL : ESP_OK;
    return handle == BLE_HS_CONN_HANDLE_NONE                       ? ESP_ERR_INVALID_STATE
           : ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM) ? ESP_FAIL
                                                                   : ESP_OK;
}
cJSON *cyd_pair_ble_status(void) {
    cJSON *o = cJSON_CreateObject();
    cJSON *a = cJSON_AddArrayToObject(o, "devices");
    if (guard)
        xSemaphoreTake(guard, portMAX_DELAY);
    cJSON_AddBoolToObject(o, "enabled", started);
    cJSON_AddBoolToObject(o, "ready", synced);
    cJSON_AddBoolToObject(o, "scanning", scanning);
    cJSON_AddBoolToObject(o, "connected", connection != BLE_HS_CONN_HANDLE_NONE);
    cJSON_AddStringToObject(o, "message", message);
    cJSON_AddStringToObject(o, "peer_name", peer_name);
    for (unsigned i = 0; i < count; i++) {
        cJSON *v = cJSON_CreateObject();
        char address[24];
        const uint8_t *p = found[i].address.val;
        snprintf(address, sizeof(address), "%02x:%02x:%02x:%02x:%02x:%02x", p[5], p[4], p[3], p[2],
                 p[1], p[0]);
        cJSON_AddStringToObject(v, "address", address);
        cJSON_AddStringToObject(v, "name", found[i].name);
        cJSON_AddNumberToObject(v, "rssi", found[i].rssi);
        cJSON_AddNumberToObject(v, "index", i);
        cJSON_AddItemToArray(a, v);
    }
    if (guard)
        xSemaphoreGive(guard);
    return o;
}
#else
cJSON *cyd_pair_ble_status(void) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "message", "Bluetooth belongs to the C5 manager");
    return o;
}
esp_err_t cyd_pair_ble_scan(void) {
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t cyd_pair_ble_connect(unsigned i) {
    (void)i;
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t cyd_pair_ble_disconnect(void) {
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
