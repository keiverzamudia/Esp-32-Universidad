# TRACE-X — Evidence Reactor

Demostración educativa **autocontenida**: un ESP32 crea su propia red Wi-Fi
(`TRACE-X-EVIDENCE-REACTOR`) y sirve en `http://192.168.4.1/` una interfaz
interactiva que explica qué es la trazabilidad de evidencias en tests
automatizados con Playwright — 6 escenarios (PASS/FAIL) con capturas,
pasos ejecutados, resultado observado e inspector de evidencias.

Sin Internet, sin PC y sin servidor externo durante la exposición: todo
(web + imágenes + manifest) está **embebido en la flash del ESP32**.

## Características

- **Servidor propio**: softAP WPA2 (8 clientes), ESP-IDF 5.5 nativo,
  `esp_http_server` con caché y compresión de estado por `?since=`.
- **8 fases narrativas** dirigidas por botón físico (BOOT/GPIO0) y por la
  web: idle → escenario → esperado → educación → resultado → captura →
  pasos → evidencia → investigar.
- **Galería de evidencias** con full + thumb, overlay de detalle, estados
  PASS/FAIL con efecto flash/glitch, `prefers-reduced-motion` respetado.
- **Seguridad básica**: header `X-TraceX-Key: tracex-2026` en APIs de
  escritura, validación de nombres de imagen, LRU de sesiones, 404
  catch-all, límite de sockets.
- **Recuperación**: pulsación corta = avanzar, ≥3 s = reiniciar secuencia,
  ≥8 s = reboot; también `POST /api/reset`.

## Construir y flashear

```powershell
cd C:\Users\keiber.zamudia\Documents\esp32
$env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [System.Environment]::GetEnvironmentVariable('Path','User')

# 1) (Re)generar evidencias desde tools/scenarios.json
powershell -ExecutionPolicy Bypass -File tools\prepare_evidence.ps1

# 2) Compilar
pio run

# 3) Con el ESP32 por USB:
pio run -t upload
pio device monitor
```

Último build medido (2026-10-15):

```
RAM:   10.3% (33,672 / 327,680 B)
Flash: 54.1% (1,134,717 / 2,097,152 B)   ← incluye 12 JPEGs + web + manifest
```

## Uso en la exposición

1. Encender el ESP32 → SSID `TRACE-X-EVIDENCE-REACTOR` / clave `Reactor2026!`.
2. Teléfono → `http://192.168.4.1/`.
3. Botón BOOT para avanzar la narrativa; `#/gallery` para las evidencias.
4. Ver `docs/EXHIBITION_PROCEDURE.md` (checklist T-1 día y T-0).

## Documentación

| Documento | Contenido |
|---|---|
| `docs/TRACE_X_HARDWARE_AUDIT.md` | Auditoría de hardware/entorno (Fase 1) |
| `docs/BUILD_AND_FLASH.md` | Build, flasheo, ajuste de particiones |
| `docs/MEMORY_REPORT.md` | Desglose de flash/RAM/heap y presupuestos |
| `docs/LOAD_TEST_PLAN.md` | Rampa de carga + criterios PASS/FAIL |
| `docs/PLAYWRIGHT_EVIDENCE_AUTHENTICITY.md` | Cadena de evidencia real vs ilustrativa |
| `docs/EXHIBITION_PROCEDURE.md` | Procedimiento día del evento |
| `docs/RECOVERY_GUIDE.md` | Recuperación en 5 niveles + gestos de botón |
| `evidence_source/README.md` | Cómo importar capturas reales de Playwright |

## Estructura

```
platformio.ini / sdkconfig.defaults / partitions_trace_x.csv
src/            firmware C (main, wifi, httpd, state, evidence, gpio)
include/        trace_config.h (SSID, GPIOs, límites) + headers
web/            index.html + app.css + app.js (nativos, sin frameworks)
evidence_source/  capturas REALES del usuario (opcional) + provenance.json
evidence_build/   JPEGs optimizados + manifest (generado por prepare_evidence.ps1)
tools/          scenarios.json, prepare_evidence.ps1, load_test.ps1
scripts/        embed_assets.py (hook pre-build: genera .S de embed)
.opencode/skills/  embedded-web-ui, esp32-firmware, esp-idf-optimization,
                   embedded-load-testing
docs/           ver tabla anterior
```

## Skills de OpenCode incluidas

- **embedded-web-ui** — presupuestos y patrones de UI para flash embebida
- **esp32-firmware** — ESP-IDF nativo con PlatformIO en Windows
- **esp-idf-optimization** — flash/RAM/heap y concurrencia LWIP
- **embedded-load-testing** — rampas, p95, heap, criterios PASS/FAIL

## Estado

- ✅ Firmware compila y enlaza ([SUCCESS])
- ⏳ Pendiente: ESP32 conectado por USB → identificación real del chip
  (`esptool flash_id`), flasheo y validación con teléfonos reales
  (la placa no estaba conectada durante el desarrollo)



https://opncd.ai/share/WJFPhKkp