#include "trace_wifi.h"
#include "trace_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "tx_wifi";

static int s_clients;
static bool s_started;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT) {
        switch (id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP started");
                break;
            case WIFI_EVENT_AP_STOP:
                ESP_LOGW(TAG, "AP stopped, restarting...");
                vTaskDelay(pdMS_TO_TICKS(1000));
                tx_wifi_restart();
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)data;
                s_clients++;
                ESP_LOGI(TAG, "client connected: MAC=" MACSTR ", aid=%d, total=%d",
                         MAC2STR(e->mac), e->aid, s_clients);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)data;
                if (s_clients > 0) s_clients--;
                ESP_LOGI(TAG, "client left: MAC=" MACSTR ", reason=%d, total=%d",
                         MAC2STR(e->mac), e->reason, s_clients);
                break;
            }
            default:
                break;
        }
    }
}

esp_err_t tx_wifi_start(void) {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    esp_netif_t *netif = esp_netif_create_default_wifi_ap();
    if (!netif) return ESP_FAIL;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) return err;

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (err != ESP_OK) return err;

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.ap.ssid, TRACE_X_SSID, sizeof(wifi_config.ap.ssid));
    strlcpy((char *)wifi_config.ap.password, TRACE_X_PASSWORD, sizeof(wifi_config.ap.password));
    wifi_config.ap.ssid_len = strlen(TRACE_X_SSID);
    wifi_config.ap.channel = TRACE_X_CHANNEL;
    wifi_config.ap.max_connection = TRACE_X_MAX_CONNECTIONS;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.ssid_hidden = 0;
    wifi_config.ap.pmf_cfg.required = false;

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) return err;
    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) return err;

    s_started = true;
    ESP_LOGI(TAG, "AP '%s' up, max_conn=%d, WPA2, ch=%d",
             TRACE_X_SSID, TRACE_X_MAX_CONNECTIONS, TRACE_X_CHANNEL);
    return ESP_OK;
}

int tx_wifi_clients(void) {
    return s_clients;
}

esp_err_t tx_wifi_restart(void) {
    if (!s_started) return ESP_ERR_INVALID_STATE;
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(200));
    s_clients = 0;
    return esp_wifi_start();
}
