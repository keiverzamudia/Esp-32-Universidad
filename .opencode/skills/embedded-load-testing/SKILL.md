---
name: embedded-load-testing
description: >
  Planear y ejecutar pruebas de carga contra servidores HTTP embebidos
  (ESP32/softAP) con scripts PowerShell sin dependencias, métricas
  p50/p95/errores/heap y criterios PASS/FAIL. Úsalo cuando se pida "probar
  cuántos clientes aguanta", "load test", "estrés" o validar una demo con
  varios teléfonos antes de una exposición.
---

# embedded-load-testing — carga en servidores embebidos

## Contexto

El objetivo NO es saturar como un CDN: es encontrar el punto donde un grupo
real (3-8 teléfonos) empieza a ver errores. El servidor suele ser
softAP + httpd con 7-12 sockets; el cuello de botella típico es LWIP, no CPU.

## Fases

1. **Baseline**: `GET /` y `GET /api/state?since=0` → latencia y 200.
2. **Rampa sintética**: 2 → 4 → 6 → 8 clientes; 60-120 s por rampa.
3. **Pico de imagen**: clientes pidiendo JPEG full (~33 KB) concurrentes.
4. **Estabilidad**: 30-60 min a 4 clientes vigilando heap.
5. **Real**: 3-6 teléfonos humanos (indispensable: el navegador real
   reutiliza conexiones y hace prefetch que el sintético no imita).

## Métricas y criterios

| Métrica | Cómo | PASS |
|---|---|---|
| Tasa de error | contadores del script | < 1 % |
| p50 / p95 latencia | Stopwatch por request | p95 < 500 ms (API) |
| Throughput | ok/duración | estable entre rampas |
| Errores de socket | "Connection reset", 503 | 0 |
| Heap final | campo `heap` del estado | > 60 KB sin tendencia a la baja |
| Reinicios | log serie / reset reason | 0 |

## Script base (sin dependencias externas)

Ver `tools/load_test.ps1` de este proyecto:

- PowerShell 5.1: `ForEach-Object -Parallel` (requiere PS7 para -Parallel;
  en 5.1 secuencial por cliente con runspaces o varios jobs).
- Distribuir rutas por pesos (poll 50 %, imágenes 30 %, resto 20 %).
- Registrar latencias en `List[double]` protegida con
  `Monitor.Enter/Exit` (¡no ArrayList sin lock!).
- Salida JSON a `load_test_results.json` + resultado PASS/FAIL inline.

```powershell
powershell -ExecutionPolicy Bypass -File tools\load_test.ps1 -BaseUrl http://192.168.4.1 -Clients 6 -DurationSec 120
```

## Errores clásicos al medir ESP32

- **Medir con el mismo Wi-Fi que usa la demo** en un solo AP de 1 canal:
  la prueba válida conecta la PC al softAP del ESP32 (la ruta del escáner
  al router no existe: no hay Internet).
- Timeout corto del cliente (<2 s) → falsos fallos en rampas altas.
- Olvidar el poll de 1 s de la UI: multiplica peticiones × tiempo.
- Concluir "aguanta 8" sin prueba de estabilidad de 30+ min (fugas de heap).
- Portal cautivo del teléfono contando como "carga lenta".

## Registro obligatorio

Guardar por rampa: fecha, clientes, duración, ok, fail, p50, p95, heap
final, PASS/FAIL, versión del firmware (fw= en el log de arranque).
Tabla en `docs/LOAD_TEST_PLAN.md`.
