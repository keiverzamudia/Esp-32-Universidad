---
name: esp-idf-optimization
description: >
  Optimizar firmware ESP-IDF para flash, RAM y heap: sdkconfig, particiones,
  strings en rodata, envío por chunks sin copias, concurrencia LWIP/httpd y
  medición real. Úsalo cuando pio run reporte uso alto de RAM/Flash, cuando
  el heap caiga en runtime o cuando se prepare una exposición con pocas
  recursos en la placa.
---

# esp-idf-optimization — memoria y concurrencia

## Medir antes de optimizar

1. **Estático**: línea `pio run` → `RAM: x%`, `Flash: y%` (fuente única de verdad).
2. **Dinámico**: exponer `esp_get_free_heap_size()` y
   `esp_get_minimum_free_heap_size()` por HTTP/log cada 2 s.
3. **Por bloques**: `idf.py size` / `xtensa-esp32-elf-size -A firmware.elf`
   (o `pio run -t size`); comparar `.text` vs `.rodata`.

## Reglas de flash

- Todo `const` acabe en `.rodata` en flash: tablas, rutas, JSON, imágenes
  embebidas. Prohibido copiar payloads grandes a arreglos globales no-const.
- Embeber assets con `EMBED_FILES`/`EMBED_TXTFILES` (o hook SCons) mantiene
  el binario atómico: 1 flasheo, 0 superficie de corrupción de FS.
- Particiones: sin OTA en exposiciones → partición `factory` grande
  continua; la partición spiffs queda como colchón.
- Imágenes: JPEG ≤50 KB full / ≤10 KB thumb; calidad 62-72 y ancho 640/200
  es el punto dulce; PNG crudo nunca va al firmware.

## Reglas de RAM

- `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=n` si no hay TLS/Internet (~40-80 KB
  de flash + RAM si el bundle completo se carga).
- Enviar respuestas con `httpd_resp_send_chunk` apuntando a `.rodata`:
  cero copias, heap constante por cliente.
- Sockets: `LWIP_MAX_SOCKETS` 12-16, httpd `max_open_sockets` 7,
  `LWIP_SO_RCVBUF` 6144 para lectores lentos sin bloquear el driver.
- JSON de estado → delta (`?since=N`) de pocos bytes en vez de objeto
  completo en cada poll; convierte ~90 % de tráfico en ahorro.
- LRU para sesiones de cliente con purga; nunca crecer sin techo.
- El heap fragmenta: allocs largos (buffers) arriba, strings pequeños en
  estáticos; no hacer malloc/free por imagen servida.

## sdkconfig.defaults — checklist mínimo

```
CONFIG_ESP_WIFI_MAX_CONN=10
CONFIG_LWIP_MAX_SOCKETS=12
CONFIG_LWIP_SO_RCVBUF=6144
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=n
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y   # o 2MB según flash_id real
CONFIG_FREERTOS_HZ=100             # basta para UI/HTTP
CONFIG_HTTPD_MAX_REQ_HDR_LEN=1024
CONFIG_HTTPD_MAX_URI_LEN=64
```
Nunca editar `.pio/build/*/sdkconfig` a mano; regenerar desde defaults
(borrar build si hace falta).

## Criterios de aceptación (esta máquina)

| Métrica | Objetivo |
|---|---|
| Flash | < 70 % de la partición app |
| RAM estática | < 20 % |
| Heap libre en idle | > 180 KB |
| Heap libre tras 10 min de carga | > 60 KB |
| Latencia p95 /api/state | < 500 ms con 6 clientes |

## Anti-patrones

- Subir `CONFIG_ESP_WIFI_MAX_CONN` sin subir sockets LWIP → colas colgadas.
- `httpd_resp_send` con buffer grande allocado por petición → fragmentation.
- Log con `%s` de payloads gigantes → heap churn; usar ESP_LOG con niveles
  por build (`CONFIG_LOG_DEFAULT_LEVEL`).
- Medir solo con `pio run` sin medir heap en runtime → fugas invisibles.
