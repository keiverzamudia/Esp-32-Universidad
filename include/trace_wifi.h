#pragma once

#include "esp_err.h"

esp_err_t tx_wifi_start(void);
int tx_wifi_clients(void);
esp_err_t tx_wifi_restart(void);
