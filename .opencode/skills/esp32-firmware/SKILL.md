---
name: esp32-firmware
description: >
  Crear o mantener firmware ESP32 nativo con ESP-IDF bajo PlatformIO
  (framework espidf). Úsalo cuando el proyecto compile con pio run en un
  entorno Windows o cuando se trate de softAP, servidores httpd, tareas
  FreeRTOS, interrupciones GPIO o particiones de flash para placas ESP32.
---

# esp32-firmware — ESP-IDF nativo con PlatformIO

## Reglas duras

1. **Fijar la plataforma**: `platform = espressif32@6.12.0` en platformio.ini.
   La 7.x trae IDF 6.1 que eliminó los componentes `json` (cJSON) y `driver`
   monolítico → rompe código IDF 5.x.
2. **Framework**: `framework = espidf`. NO usar `board = nodemcu-32s` con
   arduino a menos que el usuario lo pida.
3. **PATH de Python**: en cada shell nueva, refrescar antes de `pio`:
   ```powershell
   $env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [System.Environment]::GetEnvironmentVariable('Path','User')
   ```
4. **Nunca** mezclar `esp_http_server` con el bucle `app_main` bloqueante:
   cada servicio periódico va en su propia task con `xTaskCreatePinnedToCore`.

## Estructura canónica

```
platformio.ini          # env esp32dev, espidf, build_flags
sdkconfig.defaults      # wifi/lwip/httpd/mbedtls (¡no editar sdkconfig generado!)
partitions_trace_x.csv  # tabla de particiones personalizada
include/trace_*.h       # config + prototipos
src/main.c              # app_main: init en orden wifi→httpd→gpio→health task
src/trace_*.c           # un módulo por subsistema
scripts/*.py            # extra_scripts (hooks pre-build)
```

## Patrones que funcionan

- **Servidor HTTP**: `httpd_start` con `max_open_sockets = 7`, registrar rutas
  con `.uri` exacto; devolver 404 desde un handler catch-all registrado el
  último. Validar longitud de parámetros con `httpd_req_get_url_query_len`.
- **softAP**: `esp_netif_create_default_wifi_ap` + `esp_wifi_set_config(WIFI_IF_AP, ...)`;
  `max_connection` ≤ `CONFIG_ESP_WIFI_MAX_CONN` (10 por defecto en IDF 5.5).
- **Botón físico**: GPIO input + pull-up, interrupción `GPIO_INTR_NEGEDGE`,
  cola (`xQueue`) hacia la task que decide; debounce con `esp_timer_get_time()`
  y umbrales corto/largo/muy largo.
- **LED**: LEDC en modo determinístico; si `ledc_*` o `gpio_config` falla,
  degradar (log + seguir sin LED) — nunca abortar el arranque.
- **Estado compartido**: protégelo con `portMUX_TYPE`/`taskENTER_CRITICAL`
  o mutex; publicar eventos por cola, no tocar variables desde ISR más allá
  de encolar.
- **Embed de archivos**: ver skill `embedded-web-ui` para el hook
  `scripts/embed_assets.py` (los custom commands de CMake NO corren en el
  flujo SCons de PlatformIO).

## Errores típicos y su causa

| Error | Causa | Fix |
|---|---|---|
| `undefined reference a _binary_<path>_start` | Símbolos son basename-based: `_binary_app_css_start`, no `_binary_web_app_css_start` | Corregir nombres en C |
| `Source '.pio\build\...\.S' not found` | CMake no ejecuta `data_file_embed_asm.cmake` en flujo SCons | Hook pre-build (scripts/embed_assets.py) |
| `component 'json' not found` | Plataforma 7.x / IDF 6.1 | Fijar espressif32@6.12.0 |
| venv IDF: `windows-curses` falla al instalar | Solo afecta a `menuconfig` interactivo | `pip install esp-idf-kconfig --no-deps` + idf-component-manager + kconfiglib + cryptography |
| TXT embed con `\0` final rompe JSON | `FILE_TYPE=TEXT` añade byte nulo | longitud = end - start - 1 |

## Verificación obligatoria

```powershell
pio run   # debe terminar [SUCCESS]; anotar RAM%/Flash%
```
Si cambias memoria o rutas de embed: borrar `.pio\build` y recompilar limpio.
