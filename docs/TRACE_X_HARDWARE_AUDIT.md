# TRACE-X — Auditoría de Hardware

**Fecha:** 2026-10-15
**Entorno:** Windows 11 (keiber.zamudia), ESP32 con etiqueta `FCC ID: 2A53N-ESP32`

## 1. Dispositivo identificado

| Campo | Valor | Método |
|---|---|---|
| Etiqueta física | `ESP32`, `FCC ID: 2A53N-ESP32` | Inspección visual del usuario |
| FCC ID `2A53N-ESP32` | **Sin coincidencia** en la base de datos FCC (fccid.io, fcc.report, fcc.gov) | Búsqueda web 2026-10-15 |
| Interpretación | Placa genérica/devkit comercial sin certificación FCC verificable; el código no corresponde a un grantee registrado |
| Chip exacto | **PENDIENTE** — no se pudo identificar electrónicamente | El ESP32 **no está conectado por USB** en este momento (solo existen puertos COM Bluetooth y AMT SOL) |
| Flash | **PENDIENTE** — `esptool flash_id` no se ha podido ejecutar | Requiere conexión USB |
| MAC / eFuse | **PENDIENTE** | Requiere conexión USB |

**Decisión tomada:** se construye para el objetivo genérico `esp32dev` (ESP32 clásico WROOM/WROVER, 4 MB flash declarada por defecto en PlatformIO). Cuando el dispositivo se conecte por USB se ejecutará:

```
pio device detect
pio run -t upload
# o bien, para identificación pura:
python -m esptool --chip esp32 chip_id
python -m esptool --chip esp32 flash_id
python -m esptool --chip esp32 read_mac
```

Si la flash resultara ser de 2 MB, habrá que reducir la partición `factory` (ver §6). Si fuera de 8 MB, sobra sitio.

## 2. Herramientas disponibles (medido, no supuesto)

| Herramienta | Estado | Ruta / versión |
|---|---|---|
| Python (real) | ✅ Instalado durante la fase 1 | scoop `python 3.15.0` |
| pip | ✅ | 26.2.1 |
| PlatformIO Core | ✅ Instalado | 6.2.0 |
| PlatformIO espressif32 | ✅ Descargado en build | 6.12.0 → ESP-IDF **5.5.0** |
| Xtensa GCC | ✅ Paquete PIO | 14.2.0+20241119 |
| esptool.py | ✅ vía PIO | 4.9.0 |
| Node.js | ✅ (no usado por el firmware) | 26.9.0 |
| Git | ✅ | 2.55.0 |
| scoop / winget | ✅ | Gestores disponibles |
| ESP-IDF standalone | ❌ No instalado | Se usa el IDF 5.5 embebido en PlatformIO |
| Arduino CLI | ❌ No instalado | No necesario: se eligió ESP-IDF nativo |
| microSD / lector | ❌ El usuario declara no disponer | Evidencias van en la flash interna |

### Notas de instalación (reproducibles)

1. `scoop install python` — el alias de Microsoft Store estaba presente pero **no** había Python real.
2. `python -m pip install -U platformio`.
3. El venv de IDF (`~/.platformio/penv/.espidf-5.5.0`) requirió manualmente:
   - `idf-component-manager`, `kconfiglib`, `esp-idf-kconfig --no-deps` (por conflicto de `windows-curses` con Python 3.15; solo afecta al menú interactivo `menuconfig`, no a la compilación).
   - `cryptography` (necesaria para generar el bundle TLS de mbedTLS, obligatorio aunque no se use Internet).
4. El build se fija a `espressif32@6.12.0` porque la plataforma 7.x trae IDF 6.1, que **eliminó** los componentes `json` (cJSON) y `driver` monolítico — rompe el código IDF 5.x.

## 3. Memoria

| Recurso | Capacidad | Uso del firmware TRACE-X | Disponible |
|---|---|---|---|
| Flash total (declarada) | 4 MB | — | — |
| Partición `nvs` | 24 KB | Arranque/estados | 24 KB |
| Partición `phy_init` | 4 KB | Calibración RF | 4 KB |
| Partición `factory` (app) | **2 MB** (0x200000) | **1,134,717 B = 54.1%** | ~963 KB libres |
| Partición `spiffs` | ~960 KB | Reservada (no usada; evidencias embebidas en app) | ~960 KB |
| RAM total | 327,680 B | **33,672 B = 10.3%** estáticos | ~294 KB + heap dinámico |
| Heap libre tras arranque | medido en runtime (`/api/state`) | típicamente >200 KB en AP+httpd | — |

**Por qué `factory` única sin OTA:** una exposición no necesita actualizaciones por aire; a cambio ganamos 2 MB continuos para firmware + web + evidencias. Menos complejidad, arranque más simple.

## 4. Evidencias: comparación de opciones (Fase 4)

| Opción | Pros | Contras | Decisión |
|---|---|---|---|
| A. Embebidas en el firmware (`EMBED_FILES`) | Compilación atómica e idéntica al flashear; sin sistema de archivos que pueda corromperse; una sola operación de flasheo; inmutable durante la exposición | Cambiar imágenes = recompilar | **ELEGIDA** |
| B. Partición SPIFFS/LittleFS + `uploadfs` | Actualizable sin recompilar | Dos operaciones de flasheo; riesgo de que el usuario olvide `uploadfs`; fragmentación SPIFFS; más superficie de fallo el día del evento | Descartada (se conserva la partición spiffs vacía como colchón) |

Presupuesto real de evidencias embebidas: **227.5 KB** de imágenes (12 JPEG) + 10 KB manifest.

## 5. Wi-Fi / concurrencia

| Parámetro | Valor | Origen |
|---|---|---|
| Máx. conexiones STA del driver (`CONFIG_ESP_WIFI_MAX_CONN`) | 10 | Límite del controlador IDF 5.5 |
| `max_connection` del softAP | **8** | Elegido con margen: LWIP sockets=12, httpd sockets=7; validar en pruebas de carga |
| Seguridad | WPA2-PSK (AES), PMF no obligatoria | Compatibilidad con teléfonos |
| Dirección | `192.168.4.1:80` | Default estable del softAP de ESP-IDF |
| SSID / clave | `TRACE-X-EVIDENCE-REACTOR` / `Reactor2026!` | Cambiables en `include/trace_config.h` |

**No se promete** un número de clientes hasta validar con pruebas reales (Fase 8). Config objetivo: 8 estaciones estables con prioridad a navegación + imágenes por demanda.

## 6. Riesgos y decisiones técnicas

| Riesgo | Mitigación |
|---|---|
| Flash real ≠ 4 MB | Verificar con `esptool flash_id` antes del evento; si es 2 MB, reducir `factory` a 1.5 MB y bajar calidad de JPEGs (script ya acepta parámetros) |
| Botón BOOT (GPIO0) y LED (GPIO2) pueden no existir en la placa | Ambos se configuran en runtime; si `gpio_config` falla, el firmware **sigue funcionando solo con la web** (Fase 6 cumplida como tolerancia) |
| Placa genérica sin marca FCC | Irrelevante para el funcionamiento; el chip real se identificará con esptool al conectar |
| Python 3.15 muy nuevo para algunos paquetes IDF | Se resolvió instalando `esp-idf-kconfig --no-deps`; documentado para repetir |
| Publico conectado al AP sin Internet | Portal cautivo no implementado (teléfonos mostrarán "sin Internet", la URL `192.168.4.1` funciona igual); instrucción clara en la exposición |

## 7. Checklist antes del evento

- [ ] Conectar ESP32 por USB → `pio device detect` → confirmar chip y flash
- [ ] Si flash ≠ 4 MB → ajustar `sdkconfig.defaults` y `partitions_trace_x.csv`
- [ ] `pio run -t upload` y `pio device monitor` → verificar `TRACE-X ready -> http://192.168.4.1/`
- [ ] Conectar un teléfono al AP → abrir `http://192.168.4.1/`
- [ ] Ejecutar `tools/load_test.ps1` con 2/4/6/8 clientes sintéticos
- [ ] (Opcional) Sustituir imágenes ilustrativas por capturas reales de Playwright en `evidence_source/` + `provenance.json` y recompilar
