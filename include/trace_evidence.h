#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    const char *id;
    const char *name;
    const char *status;
    const char *explanation;
    const char *expected;
    const char *observed;
    const char *source;
    const char *note;
    int authentic;
    const char *full_name;
    const char *thumb_name;
    const uint8_t *full_data;
    size_t full_size;
    const uint8_t *thumb_data;
    size_t thumb_size;
    const char *steps_json;
    const char *related_json;
} tx_evidence_t;

esp_err_t tx_evidence_init(void);
int tx_evidence_count(void);
const tx_evidence_t *tx_evidence_get(int idx);
const tx_evidence_t *tx_evidence_find(const char *id);
const char *tx_evidence_manifest_json(size_t *len);
const uint8_t *tx_evidence_image_data(const char *name, size_t *size, const char **ctype);
