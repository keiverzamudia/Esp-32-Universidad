#include "trace_httpd.h"
#include "trace_config.h"
#include "trace_state.h"
#include "trace_evidence.h"
#include "trace_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "tx_httpd";

extern const unsigned char _binary_index_html_start[] asm("_binary_index_html_start");
extern const unsigned char _binary_index_html_end[] asm("_binary_index_html_end");
extern const unsigned char _binary_app_css_start[] asm("_binary_app_css_start");
extern const unsigned char _binary_app_css_end[] asm("_binary_app_css_end");
extern const unsigned char _binary_app_js_start[] asm("_binary_app_js_start");
extern const unsigned char _binary_app_js_end[] asm("_binary_app_js_end");

static httpd_handle_t s_server;

static esp_err_t send_text(httpd_req_t *req, const char *type, const char *data, size_t len) {
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, data, len);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "send failed: %s", esp_err_to_name(err));
    }
    return err;
}

static bool valid_ctrl_key(httpd_req_t *req) {
    char buf[64] = {0};
    if (httpd_req_get_hdr_value_len(req, TRACE_X_CTRL_KEY_HDR) == 0) return false;
    if (httpd_req_get_hdr_value_str(req, TRACE_X_CTRL_KEY_HDR, buf, sizeof(buf)) != ESP_OK) return false;
    return strcmp(buf, TRACE_X_CTRL_KEY) == 0;
}

static bool safe_image_name(const char *name, size_t len) {
    if (len == 0 || len > 40) return false;
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.')) {
            return false;
        }
    }
    if (strstr(name, "..")) return false;
    return true;
}

static esp_err_t handler_root(httpd_req_t *req) {
    size_t len = _binary_index_html_end - _binary_index_html_start - 1;
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, (const char *)_binary_index_html_start, len);
}

static esp_err_t handler_css(httpd_req_t *req) {
    size_t len = _binary_app_css_end - _binary_app_css_start - 1;
    httpd_resp_set_type(req, "text/css; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=3600");
    return httpd_resp_send(req, (const char *)_binary_app_css_start, len);
}

static esp_err_t handler_js(httpd_req_t *req) {
    size_t len = _binary_app_js_end - _binary_app_js_start - 1;
    httpd_resp_set_type(req, "application/javascript; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=3600");
    return httpd_resp_send(req, (const char *)_binary_app_js_start, len);
}

static void add_state_json(cJSON *root) {
    cJSON_AddStringToObject(root, "project", "TRACE-X");
    cJSON_AddStringToObject(root, "tagline", "Un fallo no es el final. Es el comienzo de la investigacion.");
    cJSON_AddStringToObject(root, "fw", TRACE_X_FW_VERSION_STR);
    cJSON_AddNumberToObject(root, "v", tx_state_version());
    cJSON_AddStringToObject(root, "phase", tx_phase_name(tx_state_phase()));
    cJSON_AddNumberToObject(root, "phase_elapsed_ms", (double)tx_state_phase_elapsed_ms());
    cJSON_AddNumberToObject(root, "scenario_idx", tx_state_scenario_idx());
    cJSON_AddNumberToObject(root, "scenario_count", tx_evidence_count());
    cJSON_AddNumberToObject(root, "clients", tx_wifi_clients());
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "min_free_heap", esp_get_minimum_free_heap_size());

    const tx_evidence_t *ev = tx_evidence_get(tx_state_scenario_idx());
    if (ev) {
        cJSON *sc = cJSON_AddObjectToObject(root, "scenario");
        if (sc) {
            cJSON_AddStringToObject(sc, "id", ev->id);
            cJSON_AddStringToObject(sc, "name", ev->name);
            cJSON_AddStringToObject(sc, "status", ev->status);
        }
    }
}

static esp_err_t handler_api_state(httpd_req_t *req) {
    char qbuf[32] = {0};
    int since = -1;
    if (httpd_req_get_url_query_len(req) > 0 &&
        httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf)) == ESP_OK) {
        char v[16] = {0};
        if (httpd_query_key_value(qbuf, "since", v, sizeof(v)) == ESP_OK) {
            since = atoi(v);
        }
    }

    uint32_t ver = tx_state_version();
    if (since >= 0 && (uint32_t)since == ver) {
        char tiny[32];
        int n = snprintf(tiny, sizeof(tiny), "{\"v\":%lu,\"same\":1}", (unsigned long)ver);
        return send_text(req, "application/json", tiny, n);
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "{\"error\":\"oom\"}", -1);
    }
    add_state_json(root);
    char *txt = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!txt) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "{\"error\":\"oom\"}", -1);
    }
    esp_err_t err = send_text(req, "application/json", txt, strlen(txt));
    free(txt);
    return err;
}

static esp_err_t handler_api_gallery(httpd_req_t *req) {
    size_t len = 0;
    const char *json = tx_evidence_manifest_json(&len);
    return send_text(req, "application/json", json, len);
}

static esp_err_t handler_api_evidence(httpd_req_t *req) {
    char id[32] = {0};
    const char *uri = req->uri;
    const char *prefix = "/api/evidence/";
    if (strncmp(uri, prefix, strlen(prefix)) != 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return send_text(req, "application/json", "{\"error\":\"bad path\"}", -1);
    }
    const char *rid = uri + strlen(prefix);
    size_t idlen = strcspn(rid, "?");
    if (idlen == 0 || idlen >= sizeof(id)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return send_text(req, "application/json", "{\"error\":\"bad id\"}", -1);
    }
    memcpy(id, rid, idlen);
    id[idlen] = 0;
    if (!safe_image_name(id, idlen)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return send_text(req, "application/json", "{\"error\":\"invalid id\"}", -1);
    }

    const tx_evidence_t *ev = tx_evidence_find(id);
    if (!ev) {
        httpd_resp_set_status(req, "404 Not Found");
        return send_text(req, "application/json", "{\"error\":\"not found\"}", -1);
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "{\"error\":\"oom\"}", -1);
    }
    cJSON_AddStringToObject(root, "id", ev->id);
    cJSON_AddStringToObject(root, "name", ev->name);
    cJSON_AddStringToObject(root, "status", ev->status);
    cJSON_AddStringToObject(root, "explanation", ev->explanation);
    cJSON_AddStringToObject(root, "expected", ev->expected);
    cJSON_AddStringToObject(root, "observed", ev->observed);
    cJSON_AddStringToObject(root, "source", ev->source);
    cJSON_AddStringToObject(root, "note", ev->note);
    cJSON_AddBoolToObject(root, "authentic", ev->authentic);
    if (ev->steps_json) {
        cJSON *steps = cJSON_Parse(ev->steps_json);
        if (steps) cJSON_AddItemToObject(root, "steps", steps);
    }
    if (ev->related_json) {
        cJSON *rel = cJSON_Parse(ev->related_json);
        if (rel) cJSON_AddItemToObject(root, "related", rel);
    }
    char *txt = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!txt) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "{\"error\":\"oom\"}", -1);
    }
    esp_err_t err = send_text(req, "application/json", txt, strlen(txt));
    free(txt);
    return err;
}

static esp_err_t handler_img(httpd_req_t *req) {
    const char *prefix = "/img/";
    if (strncmp(req->uri, prefix, strlen(prefix)) != 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return send_text(req, "application/json", "{\"error\":\"bad path\"}", -1);
    }
    char name[48] = {0};
    const char *rname = req->uri + strlen(prefix);
    size_t nlen = strcspn(rname, "?");
    if (nlen == 0 || nlen >= sizeof(name) || !safe_image_name(rname, nlen)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return send_text(req, "application/json", "{\"error\":\"invalid name\"}", -1);
    }
    memcpy(name, rname, nlen);
    name[nlen] = 0;

    size_t size = 0;
    const char *ctype = NULL;
    const uint8_t *data = tx_evidence_image_data(name, &size, &ctype);
    if (!data || size == 0) {
        httpd_resp_set_status(req, "404 Not Found");
        return send_text(req, "application/json", "{\"error\":\"image not found\"}", -1);
    }
    httpd_resp_set_type(req, ctype);
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    esp_err_t err = httpd_resp_send(req, (const char *)data, size);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "img send %s failed: %s", name, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t handler_advance(httpd_req_t *req) {
    if (!valid_ctrl_key(req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        return send_text(req, "application/json", "{\"error\":\"bad key\"}", -1);
    }
    tx_event_post(TX_EVENT_ADVANCE);
    send_text(req, "application/json", "{\"ok\":true}", -1);
    return ESP_OK;
}

static esp_err_t handler_reset(httpd_req_t *req) {
    if (!valid_ctrl_key(req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        return send_text(req, "application/json", "{\"error\":\"bad key\"}", -1);
    }
    tx_event_post(TX_EVENT_RESET);
    send_text(req, "application/json", "{\"ok\":true}", -1);
    return ESP_OK;
}

static esp_err_t handler_404(httpd_req_t *req) {
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"error\":\"not found\"}", -1);
}

esp_err_t tx_httpd_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = TRACE_X_HTTP_PORT;
    config.ctrl_port = TRACE_X_HTTP_PORT + 1;
    config.max_open_sockets = TRACE_X_HTTPD_MAX_SOCKETS;
    config.backlog_conn = TRACE_X_BACKLOG;
    config.stack_size = TRACE_X_HTTPD_STACK;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 16;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = handler_root},
        {.uri = "/app.css", .method = HTTP_GET, .handler = handler_css},
        {.uri = "/app.js", .method = HTTP_GET, .handler = handler_js},
        {.uri = "/api/state", .method = HTTP_GET, .handler = handler_api_state},
        {.uri = "/api/gallery", .method = HTTP_GET, .handler = handler_api_gallery},
        {.uri = "/api/evidence/*", .method = HTTP_GET, .handler = handler_api_evidence},
        {.uri = "/img/*", .method = HTTP_GET, .handler = handler_img},
        {.uri = "/api/advance", .method = HTTP_POST, .handler = handler_advance},
        {.uri = "/api/reset", .method = HTTP_POST, .handler = handler_reset},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        err = httpd_register_uri_handler(s_server, &routes[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "register %s failed: %s", routes[i].uri, esp_err_to_name(err));
            return err;
        }
    }

    httpd_uri_t not_found = {.uri = "/*", .method = HTTP_ANY, .handler = handler_404};
    httpd_register_uri_handler(s_server, &not_found);

    ESP_LOGI(TAG, "httpd on :%d, max_sockets=%d", config.server_port, config.max_open_sockets);
    return ESP_OK;
}
