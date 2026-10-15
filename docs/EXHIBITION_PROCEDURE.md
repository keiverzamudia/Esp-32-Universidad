# TRACE-X — Procedimiento de exposición (día del evento)

## T-1 día

- [ ] `pio run` → build SUCCESS; guardar copia de `firmware.bin` en USB como respaldo.
- [ ] Regenerar evidencias si hubo cambios: `powershell -ExecutionPolicy Bypass -File tools\prepare_evidence.ps1`.
- [ ] Batería: ESP32 alimentado por un cargador USB de ≥1 A estable (no por el puerto de un portátil con ahorro de energía).
- [ ] Asegurar alimentación del router no hace falta: el ESP32 crea su propio AP.

## T-0 (30 min antes)

1. Encender el ESP32 → LED GPIO2 parpadea → aparece `TRACE-X-EVIDENCE-REACTOR` en la lista Wi-Fi.
2. Conectar el teléfono anfitrión al AP:
   - SSID: `TRACE-X-EVIDENCE-REACTOR`
   - Clave: `Reactor2026!`
3. Abrir `http://192.168.4.1/` (los teléfonos Android pueden decir "sin Internet" — es correcto; la URL funciona igual).
4. Verificar: pantalla de arranque TRACE-X, contador de fases, galería con 6 evidencias.
5. Pulsar una vez el botón BOOT (GPIO0) → avanza de fase; LED rojo/verde cambia según PASS/FAIL.

## Durante la demo

- **Proyectar** desde el teléfono conectado al AP o desde una PC en el mismo AP (máx. 8 clientes).
- Flujo sugerido: `#/` → explicación → botón físico → `#/present` → `#/gallery` → abrir `ev02` (timeout) → `#/evidence/ev02` con inspector de pasos.
- Restaurar en cualquier momento: `POST /api/reset` (o mantener BOOT 3 s) → vuelve a fase 0 y re-apaga el AP si estaba en modo recuperación.

## Si falla algo (ver docs/RECOVERY_GUIDE.md)

| Síntoma | Acción inmediata |
|---|---|
| No aparece el SSID | Corte de alimentación 5 s → reinicio; si persiste, re-flashear desde la PC |
| Página no carga | Verificar IP `192.168.4.1`; olvidar y reconectar la red |
| Un cliente no conecta | SoftAP admite 8; desconectar otro; reinicio si el driver se colgó |
| Página desactualizada | El poll `?since=` puede estar congelado; recargar con Ctrl+F5 / borrar cache del navegador |
| Todo falla | Re-flashear `pio run -t upload` (5 min), o usar el `.bin` de respaldo con esptool |

## Después

- [ ] Apagar ESP32.
- [ ] Guardar `load_test_results.json` y capturas de pantalla de la demo en `docs/`.
- [ ] Si se capturaron evidencias reales durante el evento → importarlas (docs/PLAYWRIGHT_EVIDENCE_AUTHENTICITY.md).
