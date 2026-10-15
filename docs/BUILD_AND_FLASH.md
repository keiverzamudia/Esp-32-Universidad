# TRACE-X — Construcción y flasheo (Windows)

## Requisitos ya verificados en esta máquina

- Python 3.15 (scoop) + pip
- PlatformIO Core 6.2.0
- Plataforma `espressif32@6.12.0` (ESP-IDF 5.5.0) — ya descargada

## Compilar

```powershell
cd C:\Users\keiber.zamudia\Documents\esp32
# Refrescar PATH si la sesión es nueva:
$env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [System.Environment]::GetEnvironmentVariable('Path','User')

# 1) Regenerar evidencias (si cambiaste scenarios.json o añadiste capturas)
powershell -ExecutionPolicy Bypass -File tools\prepare_evidence.ps1

# 2) Compilar
pio run
```

Salida esperada (valores reales de este build):

```
RAM:   [=         ]  10.3% (used 33672 bytes from 327680 bytes)
Flash: [=====     ]  54.1% (used 1134717 bytes from 2097152 bytes)
======================== [SUCCESS] ========================
```

## Flashear (requiere ESP32 conectado por USB)

```powershell
# Identificar chip y flash (hazlo la primera vez):
pio device detect
python -m esptool --chip esp32 flash_id

# Flashear:
pio run -t upload

# Monitor serie (115200):
pio device monitor
```

Línea de éxito en el monitor:

```
TRACE-X Evidence Reactor starting, fw=1.0.0
TRACE-X ready -> http://192.168.4.1/ (heap after init: ~210000)
```

## Si la flash no es de 4 MB

1. `python -m esptool --chip esp32 flash_id` → lee el tamaño real.
2. Edita `partitions_trace_x.csv` para que `factory + spiffs` quepan en el tamaño real.
3. Edita `sdkconfig.defaults`: `CONFIG_ESPTOOLPY_FLASHSIZE_2MB` (u 8MB).
4. Borra `.pio\build` y recompila.

## Estructura del proyecto

```
esp32/
├── platformio.ini              # entorno esp32dev + IDF 5.5 + embed hook
├── partitions_trace_x.csv      # nvs + phy + factory 2MB + spiffs
├── sdkconfig.defaults          # wifi 10 conexiones, lwip 12 sockets, sin bundle TLS
├── include/trace_config.h      # SSID, clave, GPIOs, límites
├── src/                        # firmware C (IDF nativo)
│   ├── main.c                  # arranque, health task, eventos
│   ├── trace_wifi.c            # softAP WPA2, reconexión
│   ├── trace_httpd.c           # servidor HTTP + rutas + seguridad
│   ├── trace_state.c           # máquina de 8 fases + debounce
│   ├── trace_evidence.c        # manifiesto + imágenes embebidas
│   └── trace_gpio.c            # botón BOOT + LED (LEDC)
├── web/                        # interfaz (HTML/CSS/JS nativos, ~28 KB)
├── evidence_source/            # capturas REALES del usuario (opcional)
├── evidence_build/             # JPEGs optimizados + manifest (generado)
├── tools/
│   ├── prepare_evidence.ps1    # pipeline de evidencias (System.Drawing)
│   ├── scenarios.json          # definición de los 6 escenarios
│   └── load_test.ps1           # prueba de carga sintética
├── scripts/embed_assets.py     # hook pre-build: genera .S de embed (PIo+IDF)
└── docs/                       # esta documentación
```
