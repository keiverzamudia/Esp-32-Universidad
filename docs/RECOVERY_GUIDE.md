# TRACE-X — Guía de recuperación

Orden de intervención: de menor a mayor. No saltar a re-flashear si el problema es solo de red.

## Nivel 1 — El visitante no puede entrar (0-10 s)

| Síntoma | Causa probable | Acción |
|---|---|---|
| No ve el SSID `TRACE-X-EVIDENCE-REACTOR` | ESP32 apagado / softAP caído | Reinicio de alimentación (desconectar USB 5 s) |
| Se conecta pero 192.168.4.1 no abre | Cachética o portal cautivo del teléfono | Olvidar la red, reconectar, abrir `http://192.168.4.1` a mano; si el portal se cuela, usar IP sin http o probar otro navegador |
| "Señal débil" | Interferencia 2.4 GHz | Alejar de monitores/microondas; fijar canal en `trace_config.h` (canal 6 actual) |
| Máximo de clientes | Ya hay 8 estaciones | Desconectar teléfonos sin uso; reiniciar el router… no aplica: reiniciar el ESP32 (alimentación) |

## Nivel 2 — La demo responde mal (10-60 s)

| Síntoma | Causa probable | Acción |
|---|---|---|
| UI congelada (no cambia de fase) | Poll `?since=` con sesión antigua o red saturada | Recargar la página (F5/Ctrl+5 del navegador móvil: actualizar) |
| Fase incorrecta / estado desordenado | Pulsaciones dobles durante la explicación | Pulsación larga ≥3 s en BOOT → **reinicia la secuencia** a `idle` |
| LED apagado, servidor responde | GPIO2 no existe en esta placa o LED en activo bajo | Irrelevante para la demo: seguir con la web |
| Botón no avanza | BOOT (GPIO0) no cableado / mal configurado | Usar `POST /api/reset` y avanzar por API: `Invoke-WebRequest -Method POST http://192.168.4.1/api/advance` (o añadir un botón temporal en la UI — `app.js` ya llama a `/api/advance` desde `#/present`) |
| Quiero empezar de cero ya | — | Pulsación larga 3 s **o** `POST /api/reset` |

## Nivel 3 — El firmware está mal (reinicio de software / flasheo)

| Síntoma | Acción |
|---|---|
| Se reinicia solo (bucle) | Mantener BOOT **8 s** → evento `reboot` forzado; si el bucle persiste → Nivel 4 |
| Debug por serie | `pio device monitor` → ver `reset reason`, `free heap`, errores de socket |
| Estado persistente corrupto | NVS se usa solo para calibración; un `pio run -t upload` completo lo reescribe |

## Nivel 4 — Recuperación por cable (PC con PlatformIO)

```powershell
cd C:\Users\keiber.zamudia\Documents\esp32
$env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [System.Environment]::GetEnvironmentVariable('Path','User')

# 1) Ver qué hay en la flash (identifica el chip si la placa es genérica):
python -m esptool --chip esp32 flash_id
python -m esptool --chip esp32 chip_id

# 2) Re-flashear limpio:
pio run -t upload

# 3) Verificar arranque:
pio device monitor
# Esperar: "TRACE-X ready -> http://192.168.4.1/"

# 4) Último recurso (borra todo, incluida la calibración RF):
python -m esptool --chip esp32 erase_flash
pio run -t upload
```

## Nivel 5 — Pérdida total del proyecto en la PC

1. Todo el código fuente + web + evidencias + docs está en `C:\Users\keiber.zamudia\Documents\esp32\` → copia de seguridad en USB/nube.
2. El `firmware.bin` compilado (`.pio\build\esp32dev\firmware.bin`) basta para re-flashear sin reconstruir el proyecto.
3. Las herramientas se reinstalan con dos comandos: `scoop install python` + `python -m pip install -U platformio` (la plataforma e IDF se descargan solos en el primer `pio run`).

## Botones físicos — recordatorio

| Gesto en BOOT (GPIO0) | Efecto |
|---|---|
| Pulsación corta (<3 s) | Avanzar de fase |
| Pulsación ≥3 s | Reiniciar secuencia → `idle` |
| Pulsación ≥8 s | Reboot del ESP32 |
