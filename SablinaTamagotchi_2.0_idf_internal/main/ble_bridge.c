#include "ble_bridge.h"

#include "sdkconfig.h"

#ifdef CONFIG_BT_ENABLED

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BLE_BRIDGE";

#define BLE_DEVICE_NAME_PREFIX    "Sablina"
#define BLE_PEER_TIMEOUT_MS       16000UL
#define BLE_PEER_MESSAGE_MAX_TEXT 16

static const uint8_t k_company_lo = 0xFF;
static const uint8_t k_company_hi = 0xFF;
static const uint8_t k_magic_0 = 'S';
static const uint8_t k_magic_1 = 'B';
static const uint8_t k_version = 0x01;

typedef struct {
    bool visible;
    int rssi;
    uint16_t sender_id;
    uint8_t last_seq;
    uint32_t last_seen_ms;
    char name[24];
} bridge_peer_state_t;

typedef struct {
    bool valid;
    uint16_t sender_id;
    uint8_t seq;
    char text[BLE_PEER_MESSAGE_MAX_TEXT + 1];
} bridge_msg_t;

static bridge_peer_state_t g_peer;
static bridge_msg_t g_incoming;
static bool g_incoming_pending = false;
static uint16_t g_device_id = 0;
static uint8_t g_seq = 0;
static char g_device_name[24] = BLE_DEVICE_NAME_PREFIX;
static uint8_t g_adv_manufacturer[10 + BLE_PEER_MESSAGE_MAX_TEXT];
static bool g_gap_ready = false;
static bool g_adv_data_configured = false;
static bool g_scan_rsp_configured = false;

static esp_ble_adv_data_t g_adv_data = {
    .set_scan_rsp = false,
    .include_name = false,
    .include_txpower = false,
    .min_interval = 0x20,
    .max_interval = 0x40,
    .appearance = 0x00,
    .manufacturer_len = sizeof(g_adv_manufacturer),
    .p_manufacturer_data = g_adv_manufacturer,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static esp_ble_adv_data_t g_scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x20,
    .max_interval = 0x40,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static esp_ble_adv_params_t g_adv_params = {
    .adv_int_min = 0x40,
    .adv_int_max = 0x60,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_scan_params_t g_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
};

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void sanitize_text(const char *src, char *dst, size_t dst_len)
{
    if (!dst || dst_len == 0) return;
    dst[0] = '\0';
    if (!src || !src[0]) return;

    size_t out_idx = 0;
    bool last_was_space = false;
    for (size_t i = 0; src[i] && out_idx + 1 < dst_len; ++i) {
        unsigned char ch = (unsigned char)src[i];
        if (ch < 32 || ch > 126) continue;
        if (isspace(ch)) {
            if (out_idx == 0 || last_was_space) continue;
            dst[out_idx++] = ' ';
            last_was_space = true;
            continue;
        }
        dst[out_idx++] = (char)ch;
        last_was_space = false;
    }
    while (out_idx > 0 && dst[out_idx - 1] == ' ') out_idx--;
    dst[out_idx] = '\0';
}

static void build_adv_manufacturer_payload(const char *text)
{
    char cleaned[BLE_PEER_MESSAGE_MAX_TEXT + 1];
    sanitize_text(text, cleaned, sizeof(cleaned));

    memset(g_adv_manufacturer, 0, sizeof(g_adv_manufacturer));
    g_adv_manufacturer[0] = k_company_lo;
    g_adv_manufacturer[1] = k_company_hi;
    g_adv_manufacturer[2] = k_magic_0;
    g_adv_manufacturer[3] = k_magic_1;
    g_adv_manufacturer[4] = k_version;
    g_adv_manufacturer[5] = (uint8_t)(g_device_id & 0xFF);
    g_adv_manufacturer[6] = (uint8_t)((g_device_id >> 8) & 0xFF);
    g_adv_manufacturer[7] = ++g_seq;
    g_adv_manufacturer[8] = 0;
    g_adv_manufacturer[9] = 1;
    memcpy(&g_adv_manufacturer[10], cleaned, strnlen(cleaned, BLE_PEER_MESSAGE_MAX_TEXT));
}

static bool decode_msg(const uint8_t *data, uint8_t len, bridge_msg_t *out)
{
    if (!data || !out || len < sizeof(g_adv_manufacturer)) return false;
    if (data[0] != k_company_lo || data[1] != k_company_hi) return false;
    if (data[2] != k_magic_0 || data[3] != k_magic_1 || data[4] != k_version) return false;

    memset(out, 0, sizeof(*out));
    out->sender_id = (uint16_t)(data[5] | (data[6] << 8));
    out->seq = data[7];

    size_t text_idx = 0;
    for (int i = 10; i < len && text_idx < BLE_PEER_MESSAGE_MAX_TEXT; ++i) {
      char ch = (char)data[i];
      if (ch == '\0') break;
      if ((unsigned char)ch < 32 || (unsigned char)ch > 126) continue;
      out->text[text_idx++] = ch;
    }
    out->text[text_idx] = '\0';
    out->valid = text_idx > 0;
    return out->valid;
}

static void maybe_start_advertising(void)
{
    if (g_adv_data_configured && g_scan_rsp_configured) {
        esp_ble_gap_start_advertising(&g_adv_params);
    }
}

static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            g_adv_data_configured = true;
            maybe_start_advertising();
            break;

        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            g_scan_rsp_configured = true;
            maybe_start_advertising();
            break;

        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            esp_ble_gap_start_scanning(0);
            break;

        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                uint8_t adv_len = 0;
                uint8_t *name_data = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                                              ESP_BLE_AD_TYPE_NAME_CMPL,
                                                              &adv_len);
                if (!name_data || adv_len == 0) {
                    name_data = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                                         ESP_BLE_AD_TYPE_NAME_SHORT,
                                                         &adv_len);
                }

                if (name_data && adv_len > 0) {
                    size_t copy_len = adv_len < sizeof(g_peer.name) - 1 ? adv_len : sizeof(g_peer.name) - 1;
                    memcpy(g_peer.name, name_data, copy_len);
                    g_peer.name[copy_len] = '\0';
                    if (strncmp(g_peer.name, BLE_DEVICE_NAME_PREFIX, strlen(BLE_DEVICE_NAME_PREFIX)) == 0 &&
                        strcmp(g_peer.name, g_device_name) != 0) {
                        g_peer.visible = true;
                        g_peer.rssi = param->scan_rst.rssi;
                        g_peer.last_seen_ms = now_ms();
                    }
                }

                uint8_t mfg_len = 0;
                uint8_t *mfg_data = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                                             ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE,
                                                             &mfg_len);
                bridge_msg_t incoming;
                if (decode_msg(mfg_data, mfg_len, &incoming) && incoming.sender_id != g_device_id) {
                    if (g_peer.sender_id != incoming.sender_id || g_peer.last_seq != incoming.seq) {
                        g_peer.sender_id = incoming.sender_id;
                        g_peer.last_seq = incoming.seq;
                        g_peer.rssi = param->scan_rst.rssi;
                        g_peer.visible = true;
                        g_peer.last_seen_ms = now_ms();
                        g_incoming = incoming;
                        g_incoming_pending = true;
                    }
                }
            }
            break;

        default:
            break;
    }
}

void ble_bridge_init(void)
{
    if (g_gap_ready) return;

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    g_device_id = (uint16_t)((mac[4] << 8) | mac[5]);
    snprintf(g_device_name, sizeof(g_device_name), "%s-%04X", BLE_DEVICE_NAME_PREFIX, g_device_id);
    build_adv_manufacturer_payload("boot");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(g_device_name));
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&g_adv_data));
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&g_scan_rsp_data));
    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&g_scan_params));

    g_gap_ready = true;
    ESP_LOGI(TAG, "BLE bridge ready as %s", g_device_name);
}

void ble_bridge_notify_text(const char *text)
{
    if (!g_gap_ready || !text || !text[0]) return;

    build_adv_manufacturer_payload(text);
    g_adv_data_configured = false;
    esp_ble_gap_stop_advertising();
    esp_ble_gap_config_adv_data(&g_adv_data);
    ESP_LOGI(TAG, "BLE notify text: %.16s", text);
}

bool ble_bridge_poll_text(char *from_name, size_t from_name_len, char *text, size_t text_len)
{
    if (g_peer.visible && g_peer.last_seen_ms && (now_ms() - g_peer.last_seen_ms) > BLE_PEER_TIMEOUT_MS) {
        g_peer.visible = false;
        g_peer.name[0] = '\0';
        g_peer.rssi = -127;
        g_peer.last_seen_ms = 0;
        g_peer.sender_id = 0;
    }

    if (!g_incoming_pending) return false;

    if (from_name && from_name_len > 0) {
        snprintf(from_name, from_name_len, "%s", g_peer.name[0] ? g_peer.name : "Nearby Sablina");
    }
    if (text && text_len > 0) {
        snprintf(text, text_len, "%s", g_incoming.text);
    }
    g_incoming_pending = false;
    return true;
}

bool ble_bridge_peer_visible(void)
{
    if (g_peer.visible && g_peer.last_seen_ms && (now_ms() - g_peer.last_seen_ms) > BLE_PEER_TIMEOUT_MS) {
        g_peer.visible = false;
        g_peer.name[0] = '\0';
        g_peer.rssi = -127;
        g_peer.last_seen_ms = 0;
        g_peer.sender_id = 0;
    }
    return g_peer.visible;
}

const char *ble_bridge_peer_name(void)
{
    return ble_bridge_peer_visible() ? g_peer.name : "";
}

#else  /* CONFIG_BT_ENABLED not set — provide no-op stubs */

void ble_bridge_init(void) {}
void ble_bridge_notify_text(const char *text) { (void)text; }
bool ble_bridge_poll_text(char *from_name, size_t from_name_len, char *text, size_t text_len)
{
    (void)from_name; (void)from_name_len; (void)text; (void)text_len;
    return false;
}
bool ble_bridge_peer_visible(void) { return false; }
const char *ble_bridge_peer_name(void) { return ""; }

#endif /* CONFIG_BT_ENABLED */
