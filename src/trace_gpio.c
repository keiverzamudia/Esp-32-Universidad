#include "trace_gpio.h"
#include "trace_config.h"
#include "trace_state.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "tx_gpio";

typedef enum {
    LED_MODE_IDLE = 0,
    LED_MODE_ACTIVITY,
    LED_MODE_PASS,
    LED_MODE_FAIL,
    LED_MODE_WAIT
} led_mode_t;

static volatile led_mode_t s_led_mode = LED_MODE_WAIT;
static QueueHandle_t s_btn_queue;

static void led_set(uint32_t duty) {
#if STATUS_LED_ACTIVE_LOW
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1023 - duty);
#else
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
#endif
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void led_task(void *arg) {
    int tick = 0;
    while (1) {
        led_mode_t mode = s_led_mode;
        uint32_t duty = 0;
        switch (mode) {
            case LED_MODE_IDLE:
                duty = ((tick / 20) % 2) ? 80 : 0;
                break;
            case LED_MODE_ACTIVITY:
                duty = ((tick / 4) % 2) ? 300 : 0;
                break;
            case LED_MODE_PASS:
                duty = 1023;
                break;
            case LED_MODE_FAIL:
                duty = ((tick / 2) % 2) ? 1023 : 0;
                break;
            case LED_MODE_WAIT:
            default:
                duty = ((tick / 50) % 2) ? 30 : 0;
                break;
        }
        led_set(duty);
        tick++;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void btn_isr(void *arg) {
    uint32_t press_us = (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);
    xQueueSendFromISR(s_btn_queue, &press_us, NULL);
}

static void btn_task(void *arg) {
    uint32_t press_us;
    int64_t press_start = 0;
    bool pressed = false;

    while (1) {
        if (xQueueReceive(s_btn_queue, &press_us, pdMS_TO_TICKS(100)) == pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(TRACE_X_BTN_DEBOUNCE_MS));
            if (gpio_get_level(BOOT_BTN_GPIO) == 0) {
                press_start = esp_timer_get_time();
                pressed = true;
                while (gpio_get_level(BOOT_BTN_GPIO) == 0) {
                    int64_t held = esp_timer_get_time() - press_start;
                    if (held > TRACE_X_BTN_REBOOT_HOLD_MS * 1000LL) {
                        ESP_LOGW(TAG, "very long press -> reboot");
                        tx_event_post(TX_EVENT_REBOOT);
                        pressed = false;
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
                if (pressed) {
                    int64_t held = esp_timer_get_time() - press_start;
                    if (held >= TRACE_X_BTN_LONG_PRESS_MS * 1000LL) {
                        ESP_LOGI(TAG, "long press -> reset sequence");
                        tx_event_post(TX_EVENT_RESET);
                    } else {
                        ESP_LOGI(TAG, "short press -> advance");
                        tx_event_post(TX_EVENT_ADVANCE);
                    }
                    pressed = false;
                }
            }
        }
    }
}

esp_err_t tx_gpio_init(void) {
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOOT_BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "button GPIO%d config failed (%s), continuing without button",
                 BOOT_BTN_GPIO, esp_err_to_name(err));
    } else {
        s_btn_queue = xQueueCreate(4, sizeof(uint32_t));
        err = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "isr service: %s", esp_err_to_name(err));
        }
        gpio_isr_handler_add(BOOT_BTN_GPIO, btn_isr, NULL);
        xTaskCreate(btn_task, "tx_btn", 3072, NULL, 5, NULL);
        ESP_LOGI(TAG, "BOOT button on GPIO%d ready", BOOT_BTN_GPIO);
    }

    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    err = ledc_timer_config(&tcfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ledc timer failed: %s", esp_err_to_name(err));
        return ESP_OK;
    }
    ledc_channel_config_t ccfg = {
        .gpio_num = STATUS_LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ccfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ledc channel failed: %s", esp_err_to_name(err));
        return ESP_OK;
    }
    xTaskCreate(led_task, "tx_led", 2048, NULL, 4, NULL);
    ESP_LOGI(TAG, "status LED on GPIO%d ready (active_%s)",
             STATUS_LED_GPIO, STATUS_LED_ACTIVE_LOW ? "low" : "high");
    return ESP_OK;
}

void tx_gpio_set_mode_from_state(tx_phase_t phase, const char *status) {
    if (phase == PHASE_RESULT || phase == PHASE_CAPTURE) {
        if (status && strcmp(status, "FAIL") == 0) {
            s_led_mode = LED_MODE_FAIL;
            return;
        }
        if (status && strcmp(status, "PASS") == 0) {
            s_led_mode = LED_MODE_PASS;
            return;
        }
    }
    if (phase == PHASE_IDLE) {
        s_led_mode = LED_MODE_IDLE;
    } else {
        s_led_mode = LED_MODE_ACTIVITY;
    }
}
