#include "trace_evidence.h"
#include "trace_config.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "tx_evid";

extern const unsigned char _binary_manifest_json_start[] asm("_binary_manifest_json_start");
extern const unsigned char _binary_manifest_json_end[] asm("_binary_manifest_json_end");

#define EMBED_IMG(sym) \
    extern const unsigned char _binary_##sym##_start[] asm("_binary_" #sym "_start"); \
    extern const unsigned char _binary_##sym##_end[] asm("_binary_" #sym "_end");

EMBED_IMG(ev01_full_jpg)
EMBED_IMG(ev01_thumb_jpg)
EMBED_IMG(ev02_full_jpg)
EMBED_IMG(ev02_thumb_jpg)
EMBED_IMG(ev03_full_jpg)
EMBED_IMG(ev03_thumb_jpg)
EMBED_IMG(ev04_full_jpg)
EMBED_IMG(ev04_thumb_jpg)
EMBED_IMG(ev05_full_jpg)
EMBED_IMG(ev05_thumb_jpg)
EMBED_IMG(ev06_full_jpg)
EMBED_IMG(ev06_thumb_jpg)

typedef struct {
    const char *full_sym;
    const char *thumb_sym;
    const uint8_t *full_start;
    const uint8_t *full_end;
    const uint8_t *thumb_start;
    const uint8_t *thumb_end;
} img_pair_t;

static const img_pair_t s_images[] = {
    {"ev01_full.jpg", "ev01_thumb.jpg",
     _binary_ev01_full_jpg_start, _binary_ev01_full_jpg_end,
     _binary_ev01_thumb_jpg_start, _binary_ev01_thumb_jpg_end},
    {"ev02_full.jpg", "ev02_thumb.jpg",
     _binary_ev02_full_jpg_start, _binary_ev02_full_jpg_end,
     _binary_ev02_thumb_jpg_start, _binary_ev02_thumb_jpg_end},
    {"ev03_full.jpg", "ev03_thumb.jpg",
     _binary_ev03_full_jpg_start, _binary_ev03_full_jpg_end,
     _binary_ev03_thumb_jpg_start, _binary_ev03_thumb_jpg_end},
    {"ev04_full.jpg", "ev04_thumb.jpg",
     _binary_ev04_full_jpg_start, _binary_ev04_full_jpg_end,
     _binary_ev04_thumb_jpg_start, _binary_ev04_thumb_jpg_end},
    {"ev05_full.jpg", "ev05_thumb.jpg",
     _binary_ev05_full_jpg_start, _binary_ev05_full_jpg_end,
     _binary_ev05_thumb_jpg_start, _binary_ev05_thumb_jpg_end},
    {"ev06_full.jpg", "ev06_thumb.jpg",
     _binary_ev06_full_jpg_start, _binary_ev06_full_jpg_end,
     _binary_ev06_thumb_jpg_start, _binary_ev06_thumb_jpg_end},
};

static tx_evidence_t s_items[TRACE_X_MAX_EVIDENCE_ITEMS];
static int s_count;
static char *s_manifest;
static size_t s_manifest_len;

static const char *json_str_or_empty(const cJSON *obj, const char *key) {
    const cJSON *f = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);
    if (cJSON_IsString(f) && f->valuestring) return f->valuestring;
    return "";
}

static const char *json_arr_raw(const cJSON *obj, const char *key) {
    const cJSON *f = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);
    if (cJSON_IsArray(f)) {
        char *out = cJSON_PrintUnformatted(f);
        return out;
    }
    return NULL;
}

static const img_pair_t *find_image(const char *name) {
    for (int i = 0; i < (int)(sizeof(s_images) / sizeof(s_images[0])); i++) {
        if (strcmp(name, s_images[i].full_sym) == 0 || strcmp(name, s_images[i].thumb_sym) == 0) {
            return &s_images[i];
        }
    }
    return NULL;
}

esp_err_t tx_evidence_init(void) {
    size_t mlen = _binary_manifest_json_end - _binary_manifest_json_start - 1;
    s_manifest = malloc(mlen + 1);
    if (!s_manifest) return ESP_ERR_NO_MEM;
    memcpy(s_manifest, _binary_manifest_json_start, mlen);
    s_manifest[mlen] = 0;
    s_manifest_len = mlen;

    cJSON *root = cJSON_ParseWithLength(s_manifest, mlen);
    if (!root) {
        ESP_LOGE(TAG, "manifest parse failed");
        return ESP_ERR_INVALID_CRC;
    }
    const cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "items");
    const cJSON *it = NULL;
    cJSON_ArrayForEach(it, items) {
        if (s_count >= TRACE_X_MAX_EVIDENCE_ITEMS) break;
        tx_evidence_t *e = &s_items[s_count];
        e->id = strdup(json_str_or_empty(it, "id"));
        e->name = strdup(json_str_or_empty(it, "name"));
        e->status = strdup(json_str_or_empty(it, "status"));
        e->explanation = strdup(json_str_or_empty(it, "explanation"));
        e->expected = strdup(json_str_or_empty(it, "expected"));
        e->observed = strdup(json_str_or_empty(it, "observed"));
        e->source = strdup(json_str_or_empty(it, "source"));
        e->note = strdup(json_str_or_empty(it, "note"));
        const cJSON *auth = cJSON_GetObjectItemCaseSensitive((cJSON *)it, "authentic");
        e->authentic = cJSON_IsTrue(auth) ? 1 : 0;

        char fulln[64] = {0};
        char thumbn[64] = {0};
        snprintf(fulln, sizeof(fulln), "%s_full.jpg", e->id);
        snprintf(thumbn, sizeof(thumbn), "%s_thumb.jpg", e->id);
        e->full_name = strdup(fulln);
        e->thumb_name = strdup(thumbn);

        const img_pair_t *pair = find_image(fulln);
        if (pair) {
            e->full_data = pair->full_start;
            e->full_size = pair->full_end - pair->full_start;
            e->thumb_data = pair->thumb_start;
            e->thumb_size = pair->thumb_end - pair->thumb_start;
        } else {
            ESP_LOGW(TAG, "no embedded image for %s", fulln);
            e->full_size = 0;
            e->thumb_size = 0;
        }

        e->steps_json = json_arr_raw(it, "steps");
        e->related_json = json_arr_raw(it, "related");
        s_count++;
    }
    cJSON_Delete(root);
    ESP_LOGI(TAG, "loaded %d evidence items, manifest %u bytes", s_count, (unsigned)s_manifest_len);
    return ESP_OK;
}

int tx_evidence_count(void) {
    return s_count;
}

const tx_evidence_t *tx_evidence_get(int idx) {
    if (idx < 0 || idx >= s_count) return NULL;
    return &s_items[idx];
}

const tx_evidence_t *tx_evidence_find(const char *id) {
    if (!id) return NULL;
    for (int i = 0; i < s_count; i++) {
        if (strcmp(id, s_items[i].id) == 0) return &s_items[i];
    }
    return NULL;
}

const char *tx_evidence_manifest_json(size_t *len) {
    if (len) *len = s_manifest_len;
    return s_manifest;
}

const uint8_t *tx_evidence_image_data(const char *name, size_t *size, const char **ctype) {
    const img_pair_t *pair = find_image(name);
    if (!pair || !size) return NULL;
    if (strcmp(name, pair->full_sym) == 0) {
        *size = pair->full_end - pair->full_start;
        if (ctype) *ctype = "image/jpeg";
        return pair->full_start;
    }
    *size = pair->thumb_end - pair->thumb_start;
    if (ctype) *ctype = "image/jpeg";
    return pair->thumb_start;
}
