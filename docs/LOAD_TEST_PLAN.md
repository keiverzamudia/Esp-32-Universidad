# TRACE-X — Plan de pruebas de carga

**Objetivo:** validar que el ESP32 sirve la demo a un grupo de exposición sin degradación ni caída de sesiones.

## 1. Configuración objetivo

| Parámetro | Valor configurado (sdkconfig.defaults + trace_config.h) |
|---|---|
| Clientes Wi-Fi (softAP `max_connection`) | 8 |
| Límite del driver IDF | 10 (`CONFIG_ESP_WIFI_MAX_CONN`) |
| Sockets LWIP (`LWIP_MAX_SOCKETS`) | 12 |
| Sockets httpd | 7 |
| `LWIP_SO_RCVBUF` | 6144 (evita bloqueo con lectores lentos) |
| Poll de la UI | 1000 ms (`/api/state?since=N`, respuesta minúscula) |

**Expectativa conservadora:** 6 clientes reales navegando con holgura; 8 en estrés controlado.

## 2. Escenario de prueba

1. Flashear firmware, encender, conectar la PC al AP `TRACE-X-EVIDENCE-REACTOR`.
2. Confirmar `http://192.168.4.1/api/state` responde 200 JSON.
3. Ejecutar en PowerShell:

```powershell
# Rampa 1: 2 clientes, 60 s
powershell -ExecutionPolicy Bypass -File tools\load_test.ps1 -Clients 2 -DurationSec 60

# Rampa 2: 4 clientes, 60 s
powershell -ExecutionPolicy Bypass -File tools\load_test.ps1 -Clients 4 -DurationSec 60

# Rampa 3: 6 clientes, 120 s  ← escenario de exposición
powershell -ExecutionPolicy Bypass -File tools\load_test.ps1 -Clients 6 -DurationSec 120

# Rampa 4 (límite): 8 clientes, 60 s
powershell -ExecutionPolicy Bypass -File tools\load_test.ps1 -Clients 8 -DurationSec 60
```

4. Cada ejecución escribe `load_test_results.json` con p50/p95/max, throughput y errores.

## 3. Criterios de aceptación

| Métrica | Umbral |
|---|---|
| Tasa de error | < 1 % |
| Latencia p95 (`/api/state`) | < 500 ms |
| Latencia p95 de imagen full (33 KB) | < 1500 ms en rampa de exposición (6) |
| Errores de socket / "Connection reset" | 0 |
| Reinicio espontáneo del ESP32 | 0 (vigilar monitor serie) |
| Heap libre tras 10 min de carga | > 60 KB (`/api/state` reporta `heap`) |

## 4. Prueba de estabilidad (overnight / antes del evento)

- 1 hora a 4 clientes con `load_test.ps1 -Clients 4 -DurationSec 3600`.
- Verificar: `heap` no decrece linealmente (fuga), sin watchdog, LED de estado sigue latiendo.

## 5. Prueba manual con teléfonos (más fiel que el sintético)

- 3-6 teléfonos reales, cada uno en la galería de evidencias abriendo fotos full.
- Comprobar en el séptimo teléfono que `#present` sigue respondiendo.
- Botón físico: pulsar mientras los teléfonos navegan → la fase avanza en <200 ms para todos (debounce de 40 ms, doble pulsación a 500 ms).

## 6. Medidas de respaldo si falla la rampa de 8

1. Reducir `CONFIG_ESP_WIFI_MAX_CONN` de 10 a 8 ya está; bajar `max_connection` softAP a 6 → la UI muestra aviso de ocupación (implementado: httpd rechaza con 503 si no hay socket).
2. Bajar poll de la UI a 1500-2000 ms vía `TRACE_UI_POLL_MS` en `app.js`.
3. Desactivar imágenes full en galería (`USE_THUMBS_ONLY`).

## 7. Registro de resultados

Ejecutar cada rampa y pegar aquí el JSON resumen:

| Fecha | Clientes | Duración | ok | fail | p50 | p95 | heap final | PASS/FAIL |
|---|---|---|---|---|---|---|---|---|
| _pendiente_ | 2 | 60 s | | | | | | |
| _pendiente_ | 4 | 60 s | | | | | | |
| _pendiente_ | 6 | 120 s | | | | | | |
| _pendiente_ | 8 | 60 s | | | | | | |

> **Estado:** el script está listo (`tools/load_test.ps1`); la ejecución requiere el ESP32 encendido y conectado por USB/Wi-Fi, pendiente de que el usuario conecte el cable.
