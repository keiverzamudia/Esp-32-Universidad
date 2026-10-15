# TRACE-X — Checklist de validación final

**Fecha de ejecución:** 2026-10-15 (build de desarrollo; hardware NO conectado)

## ✅ Validado en esta máquina

| # | Item | Evidencia |
|---|---|---|
| 1 | Firmware compila y enlaza sin errores | `pio run` → `[SUCCESS]`, 22 s incrementales |
| 2 | Flash/RAM dentro de presupuesto | Flash 54.1 % (1,134,717/2,097,152 B), RAM 10.3 % |
| 3 | Imágenes + web + manifest embebidos | 36 símbolos `_binary_*_start/end` presentes en el ELF (verificado por grep de fuentes y link exitoso) |
| 4 | Presupuestos UI (skill embedded-web-ui) | index 3,019 B ≤ 8K · css 11,417 B ≤ 16K · js 14,031 B ≤ 16K · imgs 227,537 B ≤ 350K |
| 5 | Evidencias: 6 escenarios PASS/FAIL con pasos y explicación | `evidence_build/manifest.json` → `items: 6` |
| 6 | Autenticidad marcada | `authentic: False ×6` + nota «DEMOSTRACIÓN EDUCATIVA» en cada ítem; `provenance.json` listo para imports reales |
| 7 | Pipeline regenerable | `tools\prepare_evidence.ps1` (probado), `tools\scenarios.json` |
| 8 | Hook de embed reproducible | `scripts/embed_assets.py` se ejecuta en cada `pio run` (log: "assembly files generated") |
| 9 | Script de carga sin dependencias | `tools\load_test.ps1` → sintaxis PS verificada; escribe `load_test_results.json` con p50/p95/PASS-FAIL |
| 10 | Interacción botón/LED implementada | corto=avanzar, ≥3 s=reset secuencia, ≥8 s=reboot (`trace_gpio.c:80-99`) |
| 11 | APIs de control con clave | `/api/advance`, `/api/reset` con header `X-TraceX-Key` |
| 12 | Recuperación documentada en 5 niveles | `docs/RECOVERY_GUIDE.md` |
| 13 | 4 skills de OpenCode | `.opencode/skills/{embedded-web-ui,esp32-firmware,esp-idf-optimization,embedded-load-testing}/SKILL.md` |
| 14 | 7 documentos + README | docs/: auditoría, build/flash, memoria, carga, autenticidad, exposición, recuperación |
| 15 | Compilación determinista tras cambios | 2 ejecuciones consecutivas de `pio run` → SUCCESS idéntico |

## ⏳ Pendiente de hardware (bloqueado: falta cable USB)

| # | Item | Comando / acción | Cuándo |
|---|---|---|---|
| 16 | Identificar chip y flash reales | `python -m esptool --chip esp32 flash_id`, `pio device detect` | Al conectar el cable |
| 17 | Ajustar `sdkconfig.defaults` si flash ≠ 4 MB | editar + borrar `.pio\build` | Tras (16) |
| 18 | Flashear | `pio run -t upload` | Tras (16) |
| 19 | Verificar arranque por serie | `pio device monitor` → `TRACE-X ready -> http://192.168.4.1/` | Tras (18) |
| 20 | Validar AP + UI en teléfono real | conectar a `TRACE-X-EVIDENCE-REACTOR`, abrir `http://192.168.4.1/` | Tras (18) |
| 21 | Rampa de carga 2/4/6/8 clientes | `tools\load_test.ps1` (docs/LOAD_TEST_PLAN.md) | Tras (20) |
| 22 | Estabilidad 30-60 min + heap > 60 KB | load test largo + `/api/state` | Antes del evento |
| 23 | Botón físico en placa real (GPIO0/2) | probar los 3 gestos | Tras (18) |
| 24 | (Opcional) Capturas reales de Playwright → importar | docs/PLAYWRIGHT_EVIDENCE_AUTHENTICITY.md §2 | Cuando existan |

## ❌ No realizado a propósito (decisión, no deuda)

- Portal cautivo Wi-Fi: los teléfonos indican "sin Internet"; la URL directa funciona (aceptado en Fase 7).
- OTA: partición única `factory` de 2 MB por diseño (docs/MEMORY_REPORT.md).
- Imágenes auténticas de Playwright: no hay capturas reales en el equipo; se usan ilustraciones marcadas (Fase 4/9).
