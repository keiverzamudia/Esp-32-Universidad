#include "trace_config.h"
#include "trace_state.h"
#include "trace_evidence.h"
#include "trace_wifi.h"
#include "trace_httpd.h"
#include "trace_gpio.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "tx_main";

void tx_gpio_set_mode_from_state(tx_phase_t phase, const char *status);

static void health_task(void *arg) {
    uint32_t last_ver = 0;
    while (1) {
        tx_event_t ev = tx_event_fetch();
        if (ev == TX_EVENT_ADVANCE) {
            tx_state_advance();
        } else if (ev == TX_EVENT_RESET) {
            tx_state_reset();
        } else if (ev == TX_EVENT_REBOOT) {
            ESP_LOGW(TAG, "soft reboot requested");
            esp_restart();
        }

        uint32_t ver = tx_state_version();
        if (ver != last_ver) {
            last_ver = ver;
            const tx_evidence_t *evd = tx_evidence_get(tx_state_scenario_idx());
            tx_gpio_set_mode_from_state(tx_state_phase(), evd ? evd->status : NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "TRACE-X Evidence Reactor starting, fw=%s", TRACE_X_FW_VERSION_STR);
    ESP_LOGI(TAG, "reset reason: %d, free heap: %u", esp_reset_reason(), esp_get_free_heap_size());

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "nvs erase");
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs init failed: %s", esp_err_to_name(err));
        return;
    }

    err = tx_state_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "state init failed: %s", esp_err_to_name(err));
        return;
    }

    err = tx_evidence_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "evidence init failed: %s", esp_err_to_name(err));
        return;
    }

    err = tx_gpio_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "gpio init degraded: %s", esp_err_to_name(err));
    }

    err = tx_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wifi start failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "continuing without AP - check sdkconfig");
    }

    err = tx_httpd_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed: %s", esp_err_to_name(err));
    }

    xTaskCreate(health_task, "tx_health", 4096, NULL, 6, NULL);

    ESP_LOGI(TAG, "TRACE-X ready -> http://192.168.4.1/ (heap after init: %u)",
             esp_get_free_heap_size());
}
