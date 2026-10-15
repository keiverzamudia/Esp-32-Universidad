# evidence_source — capturas originales

Coloca aquí tus **capturas reales de Playwright** (PNG o JPG) y regístralas en
`provenance.json` para que entren al firmware con su procedencia intacta:

```json
{
  "imports": [
    {
      "id": "ev02",
      "file": "ev02-login-timeout.png",
      "source": "Playwright 1.49 · tests/checkout.spec.ts · 2026-10-10",
      "note": "Captura real del runner en Chromium."
    }
  ]
}
```

- `id` debe coincidir con un escenario de `tools/scenarios.json` (ev01…ev06).
- `file` es el nombre del archivo dentro de esta carpeta.
- Sin entrada aquí, el pipeline genera una imagen ilustrativa claramente
  marcada como **DEMOSTRACIÓN EDUCATIVA** — nunca se presenta como evidencia real.

Ejecuta después: `powershell -ExecutionPolicy Bypass -File tools\prepare_evidence.ps1`
