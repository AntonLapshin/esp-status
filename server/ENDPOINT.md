# /api/esp-status — ESP-optimized endpoint spec

Live implementation: `auto-pi/ui/server/server.js` → `buildEspStatus()`.

Tiny aggregated JSON for ESP32 polling over LAN (~310 bytes, `Cache-Control: no-store`).
Full dashboards should use `/api/status` instead.

## Request

```
GET http://<DEV-LAN-IP>:8787/api/esp-status
```

## Response 200 (v7)

```json
{
  "ok": true,
  "proj": "timeline",
  "loop": true,
  "status": "green",
  "provider": "joingonka",
  "model": "deepseek-ai/DeepSeek-V4-Flash-0731",
  "ok_n": 9,
  "fail_n": 1,
  "persona": "engineer",
  "ago_s": 47,
  "at": "2026-09-13T18:32:38.206Z"
}
```

| Field | UI element | Source |
|---|---|---|
| `proj` | header: project name | active project name (max 24 chars) |
| `loop` | header: `ON` / `OFF` badge | `.pi/state/loop.lock` liveness |
| `status` | pulsating dot: `green` / `red` | green = loop on + last run healthy + (a persona actively running OR fresh activity ≤ 15 min); else red |
| `provider` | `PROVIDER` line | effective pi provider (config → `PI_*` env → pi settings → pi `auth.json` → env hint) |
| `model` | small model line under the provider (basename after `/`, max 28 chars) | effective pi model (config → `PI_*` env → pi settings → `health.jsonl` fallback) |
| `ok_n`, `fail_n` | `SUCCESS` gauge (last 10 LLM calls, e.g. `9/10 90%`) | successful / failed LLM calls over the last 10 `health.jsonl` records (`ok_n + fail_n <= 10`, fewer when less than 10 calls exist; firmware renders `ok_n / (ok_n + fail_n)`) |
| `persona` | `PERSONA` hero glyph (`PM`, `ENGINEER`, `QA`, `REVIEW`, …) | active persona (started run wins, else last run) |
| `ago_s` | freshness (`47s ago`, footer) | seconds since last run/event (`-1` = never) |

Legacy fields (`last`, `tok_today`, `err`) are still
sent so pre-v7 firmware keeps working. `succ`/`total` were removed in v7 —
they duplicated the same last-10 LLM-call window; `ok_n`/`fail_n` now carry
the LLM-call outcome directly.

## Display mapping (firmware)

- Dot: `green` = healthy loop, `red` = stopped / stale / recent error.
  `grey` is ESP-side only (WiFi/HTTP failed).
- Persona colors: PM gold, engineer cyan, QA green, review magenta.
- Gauge: green ≥ 90 %, yellow ≥ 60 %, red below. The server windows the gauge
  to the last 10 LLM calls (`ok_n + fail_n <= 10`); the firmware renders
  `ok_n / (ok_n + fail_n)` directly (clamped defensively to 0..10).

## Errors

- `404 {"error":"No active auto-pi project found."}` — no seed yet.
