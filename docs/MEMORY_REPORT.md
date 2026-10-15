# TRACE-X — Informe de memoria

**Build de referencia:** `pio run` de 2026-10-15, espressif32@6.12.0 / ESP-IDF 5.5.0, xtensa-esp-elf-gcc 14.2.0, objetivo `esp32dev`.

## 1. Resumen del enlace

```
RAM:   [=         ]  10.3% (used 33672 bytes from 327680 bytes)
Flash: [=====     ]  54.1% (used 1134717 bytes from 2097152 bytes)
```

| Recurso | Usado | Total | % libre |
|---|---|---|---|
| RAM estática (.data + .bss + .rodata en RAM si aplica) | 33,672 B | 327,680 B | 89.7 % |
| Flash (partición `factory` 2 MB) | 1,134,717 B | 2,097,152 B | 45.9 % |
| Heap dinámico tras arranque | ~200-215 KB (medir con `/api/state` → campo `heap`) | — | — |

## 2. Composición estimada de la flash (1,134,717 B)

| Bloque | Tamaño aprox. | Origen |
|---|---|---|
| Código IDF (lwip, httpd, wpa, freertos, vfs, nvs, newlib, mbedTLS…) | ~700 KB | framework-espidf 5.5.0 |
| Evidencias embebidas (12 JPEG en `.rodata`) | 227,537 B | `evidence_build/` (full 33 KB ×6, thumb 4.5 KB ×6) |
| Manifest JSON | 10,269 B | `evidence_build/manifest.json` |
| Interfaz web (HTML + CSS + JS) | 28,467 B | `web/` (3,019 + 11,417 + 14,031) |
| Firmware propio (6 módulos C) | ~30-50 KB | `src/` |
| Cabeceras, tablas de rutas, strings de log | ~10 KB | — |
| Bundle TLS x509 (presente aunque deshabilitado en config) | ~1-5 KB | generación por defecto de IDF |

> Los porcentajes de la línea de `pio run` son medidos por el enlazador; el desglose por bloques es una estimación reconciliada con los totales de `web/` y `evidence_build/` (medidos con Get-ChildItem).

## 3. Presupuestos de la UI (skill embedded-web-ui) — cumplimiento

| Artefacto | Presupuesto | Real | Estado |
|---|---|---|---|
| `index.html` | ≤ 8 KB | 3,019 B | ✅ 37 % |
| `app.css` | ≤ 16 KB | 11,417 B | ✅ 71 % |
| `app.js` | ≤ 16 KB | 14,031 B | ✅ 88 % |
| Imagen full | ≤ 50 KB | máx ~34 KB | ✅ |
| Imagen thumb | ≤ 10 KB | máx ~5 KB | ✅ |
| Total imágenes | ≤ 350 KB | 227,537 B | ✅ 65 % |

## 4. Heap y fragmentation — políticas del firmware

- `httpd` con **7 sockets**, `LWIP_MAX_SOCKETS=12`, `LWIP_SO_RCVBUF=6144` → espacio para que 8 estaciones no agoten colas.
- Servicios de imagen: puntero directo a `.rodata` embebida + `httpd_resp_send_chunk` → **cero copias** del payload en heap.
- LRU de sesiones de estado (`since`), purge cuando >32 sesiones.
- Health task cada 2 s registra `free heap` en log; verificar con `?since=` → respuesta incluye `heap`.
- Criterio de aceptación de carga: heap libre > 60 KB tras 10 min (docs/LOAD_TEST_PLAN.md §3).

## 5. Ahorro aplicado (antropometría de decisiones)

| Decisión | Ahorro |
|---|---|
| `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=n` (demo sin TLS/Internet) | ~40-80 KB flash y ~8-15 KB RAM si se hubiera cargado el bundle completo |
| Evidencias en `factory` embebidas, sin partición de datos en uso | 0 heap para caché de archivos; sin código VFS-FS |
| JPEG optimizados (q72/640px full, q62/200px thumb) | de ~1-2 MB por PNG crudo a 227 KB totales |
| Poll con `since` (delta minúsculo) en vez de JSON completo | ~90 % menos bytes por sondeo que mandar el estado entero |
| Static strings en flash vía `const` (IDF las deja en `.rodata`) | RAM estática mínima (10.3 %) |

## 6. Cómo regenerar estas cifras

```powershell
pio run | Select-String "RAM:|Flash:"
# Desglose web/evidencias:
(Get-ChildItem web | Measure-Object Length -Sum)
(Get-ChildItem evidence_build | Measure-Object Length -Sum)
# Heap en runtime (con el ESP32 en la red):
(Invoke-WebRequest "http://192.168.4.1/api/state?since=0" -UseBasicParsing).Content
```

## 7. Márgenes para crecer

- **Flash:** ~963 KB libres en `factory` → cabrían ~28 imágenes full más, o videos cortos en MP4 embebidos.
- **RAM:** 294 KB libres de estática + heap → soporta ampliar concurrencia a 10 estaciones si las pruebas lo requieren (subir `LWIP_MAX_SOCKETS` a 16).
