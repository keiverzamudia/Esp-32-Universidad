#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    PHASE_IDLE = 0,
    PHASE_SCENARIO,
    PHASE_EXPECTED,
    PHASE_EDUCATION,
    PHASE_RESULT,
    PHASE_CAPTURE,
    PHASE_STEPS,
    PHASE_RAW,
    PHASE_INVESTIGATE,
    PHASE_COUNT
} tx_phase_t;

typedef enum {
    TX_EVENT_NONE = 0,
    TX_EVENT_ADVANCE,
    TX_EVENT_RESET,
    TX_EVENT_REBOOT
} tx_event_t;

esp_err_t tx_state_init(void);
esp_err_t tx_state_advance(void);
esp_err_t tx_state_reset(void);

tx_phase_t tx_state_phase(void);
int tx_state_scenario_idx(void);
uint32_t tx_state_version(void);
int64_t tx_state_phase_elapsed_ms(void);
const char *tx_phase_name(tx_phase_t phase);

esp_err_t tx_event_post(tx_event_t ev);
tx_event_t tx_event_fetch(void);
