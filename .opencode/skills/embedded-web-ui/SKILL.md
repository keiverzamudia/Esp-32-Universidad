---
name: embedded-web-ui
description: Diseña y construye interfaces web para servidores HTTP embebidos (ESP32/ESP-IDF httpd). Aplicar cuando se crean o modifican HTML/CSS/JS que se sirven desde firmware con memoria y red muy limitadas.
---

# Embedded Web UI — interfaz brutal, única y ultraligera desde firmware

## Presupuesto innegociable (por carga inicial, sin gzip)

| Recurso | Límite |
|---|---|
| index.html | ≤ 8 KB |
| app.css | ≤ 16 KB |
| app.js | ≤ 16 KB |
| JSON estado | ≤ 400 B por respuesta |
| Imágenes | miniatura ≤ 8 KB, completa ≤ 45 KB, JPEG q≤72, ancho ≤720 px |

Prohibido en el cliente: frameworks, web fonts externas, iconos SVG externos, canvas/WebGL para decoración, `backdrop-filter`, sombras multiplicadas por elementos, requests a dominios remotos, autoplay de vídeo.

## Identidad visual (TRACE-X / Evidence Reactor)

- **Género**: laboratorio de contención nuclear + terminal de diagnóstico. No dashboard genérico.
- **Paleta**: vacío `#04070c`, paneles `rgba(10,18,28,.85)`, cian señal `#2ee6ff`, ámbar `#ffb020`, rojo fallo `#ff3b4a`, verde pase `#2bff88`, texto `#cfe8f5`, apagado `#5f8aa3`.
- **Tipografía**: pila monoespaciada del sistema (`ui-monospace, SFMono-Regular, Menlo, Consolas, monospace`). Jerarquía por tamaño, peso, tracking y mayúsculas — nunca por fuentes descargadas.
- **Firma única obligatoria**: (1) reactor con anillos punteados girando en sentidos opuestos + núcleo pulsante, solo con `transform`/`opacity`; (2) retícula técnica fija + líneas de barrido con degradados CSS; (3) paneles con corchetes de esquina (pseudo-elementos); (4) barra de fases tipo "secuencia de contención" de 8 segmentos; (5) efecto glitch de un solo disparo al entrar en presentación; (6) banner FAIL con pulso rojo + flash de viñeta una vez.
- **Accesibilidad**: `prefers-reduced-motion` desactiva animaciones; targets ≥44 px; contraste AA en texto principal.

## Arquitectura de cliente (sin framework)

- Un solo `poll` de `GET /api/state?since=<v>` cada 2,5 s (304 = sin cambios ≈ 0 B de cuerpo). Backoff a 5–10 s tras fallos.
- Vistas: `home | present | gallery` + overlay detalle. Estado en memoria; nunca recargar la página para cambiar de vista.
- Toda la narración de fases se deriva del estado del dispositivo (`phase`, `scenario`, `v`). Los clientes se sincronizan al detectar `v` nueva; si el usuario está en galería, toast en lugar de secuestrar la vista.
- Contenido de presentación renderizado por JS a partir del manifiesto/estado; el HTML solo contiene el armazón.
- Imágenes: `loading="lazy"` + `decoding="async"` en miniaturas; la completa solo se solicita al abrir detalle o en la fase `capture`.
- Control de secuencia: `POST /api/advance|reset` con header `X-TraceX-Key`; clave en `localStorage`; en 401 borrar clave y volver a pedir.

## Técnicas CSS permitidas

- Animar únicamente `transform`, `opacity` y (con moderación) `background-position`.
- Degradados radiales/conicos para el núcleo; `repeating-linear-gradient` para retícula y scanlines en capas `position:fixed; pointer-events:none`.
- Efectos HUD con `::before`/`::after` (corchetes, acentos), no con elementos extra.
- Transiciones de vista ≤ 260 ms; entrada de pasos escalonada con `animation-delay` inline (máx. 8 retrasos).

## Errores frecuentes

1. Precargar todas las capturas al abrir la galería → siempre miniaturas.
2. Poll sin `since` → desperdicia ancho de banda con 8+ clientes.
3. `setInterval` sin cancelar en `fetch` lento → solapamientos; usar cadena de `setTimeout`.
4. Olvidar UTF-8/acentos en JSON servido por firmware → declarar `charset=utf-8`.
5. Animaciones con `box-shadow` en bucle → calienta GPUs móviles; usar opacidad del color de acento.

## Verificación

- Presupuestos de KB con `Get-Item` antes de compilar.
- `Select-String` de toda URL en HTML/JS: debe salir `/`, `/app.*`, `/api/*`, `/img/*` — ninguna absoluta externa.
- Prueba manual: home → iniciar → recorrer 8 fases → galería → detalle → related → reset; luego con el teléfono en modo avión solo Wi-Fi (sin Internet) todo debe funcionar.
