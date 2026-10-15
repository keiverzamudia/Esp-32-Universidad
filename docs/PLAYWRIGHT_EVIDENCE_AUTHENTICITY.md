# TRACE-X — Autenticidad de evidencias Playwright

**Principio rector:** separar siempre lo **real** de lo **ilustrativo**. La demo es educativa; engañar al espectador sobre qué es una captura auténtica invalidaría el propósito de trazabilidad.

## 1. Estados de una evidencia

| Estado | Significado | Cómo se detecta |
|---|---|---|
| `authentic: true` | Captura real tomada de una ejecución de Playwright del usuario | Entrada presente en `evidence_source/provenance.json` con hash SHA-256 del archivo original |
| `authentic: false` (por defecto) | Imagen generada por `tools/prepare_evidence.ps1` para explicar el concepto | `imports: []` en `provenance.json`; el firmware no almacena este campo en el manifiesto (el manifiesto se regenera desde `scenarios.json`) |

En pantalla, las imágenes generadas llevan marca de agua visible **«DEMOSTRACIÓN EDUCATIVA»** (dibujada por el pipeline de prepare_evidence.ps1). Esta marca es la garantía visible para el espectador.

## 2. Cómo importar una captura real (cuando exista)

1. Ejecutar el test Playwright con trazas:
   ```bash
   npx playwright test --trace on
   # o bien en código:
   await page.screenshot({ path: 'test-results/ev01_full.png', fullPage: true })
   await context.tracing.stop({ path: 'test-results/ev01.trace.zip' })
   ```
2. Copiar el PNG/trace a `evidence_source/`.
3. Calcular el hash y registrar la procedencia en `evidence_source/provenance.json`:
   ```json
   {
     "schema": 1,
     "imports": [
       {
         "evidence_id": "ev01",
         "file": "ev01_full.png",
         "sha256": "<hash>",
         "captured_at": "2026-10-15T14:30:00-05:00",
         "playwright_version": "1.x.y",
         "browser": "chromium 1xx",
         "app_version": "v2.3",
         "test": "tests/login.spec.ts",
         "authentic": true
       }
     ]
   }
   ```
4. Añadir en `tools/scenarios.json` el campo `"source": "evidence_source/ev01_full.png"` (o colocar el archivo con el nombre que espera el pipeline).
5. Regenerar: `powershell -ExecutionPolicy Bypass -File tools\prepare_evidence.ps1` → sobrescribe `evidence_build/ev01_full.jpg` sin marca de agua para archivos con entrada `authentic: true`.
6. Recompilar y flashear: `pio run -t upload`.

## 3. Reglas de honestidad (no negociables)

- [ ] Ninguna imagen generada se presenta como captura de una app real.
- [ ] Toda captura real tiene hash + fecha + versión de la app + versión de Playwright en `provenance.json`.
- [ ] Si se pierde el original, la evidencia vuelve a estado ilustrativo (se regenera con marca de agua).
- [ ] El manifest JSON servido por el firmware refleja `source`/`authentic` cuando exista, para que la galería pueda mostrar el distintivo.

## 4. Cadena de evidencia completa (ejemplo mínimo)

```
evidence_source/ev01_full.png          ← original (captura Playwright)
provenance.json                        ← sha256 + contexto + fecha
tools/scenarios.json                   ← expected/observed/steps (qué se afirma)
evidence_build/ev01_full.jpg           ← optimizado para flash (puede heredar auth)
firmware .rodata                       ← binario inmutable con todo lo anterior
/api/evidence/ev01                     ← lo que sirve el ESP32, incl. metadatos
```

Cualquier eslabón roto (hash que no coincide, imagen sin registrar) ⇒ tratar la evidencia como ilustrativa.
