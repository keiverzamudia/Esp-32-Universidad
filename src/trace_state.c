#include "trace_state.h"
#include "trace_evidence.h"
#include "trace_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "tx_state";

static struct {
    tx_phase_t phase;
    int scenario_idx;
    uint32_t version;
    int64_t phase_enter_us;
    int64_t last_advance_us;
} s_state;

static QueueHandle_t s_evt_queue;
static SemaphoreHandle_t s_lock;

static const char *s_phase_names[PHASE_COUNT] = {
    "idle", "scenario", "expected", "education",
    "result", "capture", "steps", "raw", "investigate"
};

const char *tx_phase_name(tx_phase_t phase) {
    if (phase < 0 || phase >= PHASE_COUNT) return "unknown";
    return s_phase_names[phase];
}

esp_err_t tx_state_init(void) {
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    s_evt_queue = xQueueCreate(8, sizeof(tx_event_t));
    if (!s_evt_queue) return ESP_ERR_NO_MEM;
    s_state.phase = PHASE_IDLE;
    s_state.scenario_idx = 0;
    s_state.version = 1;
    s_state.phase_enter_us = esp_timer_get_time();
    s_state.last_advance_us = 0;
    ESP_LOGI(TAG, "state machine ready, phase=idle");
    return ESP_OK;
}

esp_err_t tx_state_advance(void) {
    const int64_t now = esp_timer_get_time();
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (now - s_state.last_advance_us < 400000) {
        xSemaphoreGive(s_lock);
        return ESP_OK;
    }
    s_state.last_advance_us = now;
    const int count = tx_evidence_count();
    if (count == 0) {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NOT_FOUND;
    }
    if (s_state.phase == PHASE_INVESTIGATE) {
        s_state.scenario_idx++;
        if (s_state.scenario_idx >= count) {
            s_state.scenario_idx = 0;
            s_state.phase = PHASE_IDLE;
        } else {
            s_state.phase = PHASE_SCENARIO;
        }
    } else {
        s_state.phase++;
    }
    s_state.version++;
    s_state.phase_enter_us = now;
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "advance -> %s (scenario %d, v%u)",
             tx_phase_name(s_state.phase), s_state.scenario_idx, s_state.version);
    return ESP_OK;
}

esp_err_t tx_state_reset(void) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state.phase = PHASE_IDLE;
    s_state.scenario_idx = 0;
    s_state.version++;
    s_state.phase_enter_us = esp_timer_get_time();
    xSemaphoreGive(s_lock);
    ESP_LOGW(TAG, "sequence reset to idle");
    return ESP_OK;
}

tx_phase_t tx_state_phase(void) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    tx_phase_t p = s_state.phase;
    xSemaphoreGive(s_lock);
    return p;
}

int tx_state_scenario_idx(void) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int i = s_state.scenario_idx;
    xSemaphoreGive(s_lock);
    return i;
}

uint32_t tx_state_version(void) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint32_t v = s_state.version;
    xSemaphoreGive(s_lock);
    return v;
}

int64_t tx_state_phase_elapsed_ms(void) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int64_t ms = (esp_timer_get_time() - s_state.phase_enter_us) / 1000;
    xSemaphoreGive(s_lock);
    return ms;
}

esp_err_t tx_event_post(tx_event_t ev) {
    if (!s_evt_queue) return ESP_ERR_INVALID_STATE;
    if (xQueueSend(s_evt_queue, &ev, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, dropping %d", ev);
    }
    return ESP_OK;
}

tx_event_t tx_event_fetch(void) {
    tx_event_t ev = TX_EVENT_NONE;
    if (s_evt_queue && xQueueReceive(s_evt_queue, &ev, 0) == pdTRUE) {
        return ev;
    }
    return TX_EVENT_NONE;
}
