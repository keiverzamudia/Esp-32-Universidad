#pragma once

#include "esp_err.h"
#include "trace_state.h"

esp_err_t tx_gpio_init(void);
void tx_gpio_set_mode_from_state(tx_phase_t phase, const char *status);
