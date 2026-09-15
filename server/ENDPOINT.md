# /api/esp-status — ESP-optimized endpoint spec

Live implementation: `auto-pi/ui/server/server.js` → `buildEspStatus()`.

Tiny aggregated JSON for ESP32 polling over LAN (~310 bytes, `Cache-Control: no-store`).
Full dashboards should use `/api/status` instead.

## Request

```
GET http://<DEV-LAN-IP>:8787/api/esp-status
```

## Response 200 (v5)

```json
{
  "ok": true,
  "proj": "timeline",
  "loop": true,
  "status": "green",
  "provider": "joingonka",
  "model": "deepseek-ai/DeepSeek-V4-Flash-0731",
  "succ": 9,
  "total": 10,
  "persona": "engineer",
  "ago_s": 47,
  "runs": 10,
  "ok_n": 9,
  "fail_n": 1,
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
| `succ`, `total` | `SUCCESS` gauge (last 10 calls, e.g. `9/10 90%`) | successful / total LLM calls over the last 10 `health.jsonl` records (`total` capped at 10, windowed server-side; firmware renders directly) |
| `runs`, `ok_n`, `fail_n` | legacy run counters (last 10 runs) | last 10 `runs.jsonl` records (`runs` capped at 10, `ok_n`/`fail_n` counted within that window) |
| `persona` | `PERSONA` hero glyph (`PM`, `ENGINEER`, `QA`, `REVIEW`, …) | active persona (started run wins, else last run) |
| `ago_s` | freshness (`47s ago`, footer) | seconds since last run/event (`-1` = never) |

Legacy fields (`last`, `runs`, `ok_n`, `fail_n`, `tok_today`, `err`) are still
sent so pre-v5 firmware keeps working, but v5 firmware renders the gauge from
the server-windowed `succ`/`total` (resp. `ok_n`/`fail_n`).

## Display mapping (firmware)

- Dot: `green` = healthy loop, `red` = stopped / stale / recent error.
  `grey` is ESP-side only (WiFi/HTTP failed).
- Persona colors: PM gold, engineer cyan, QA green, review magenta.
- Gauge: green ≥ 90 %, yellow ≥ 60 %, red below. The server windows the gauge
  to the last 10 provider calls (`total` capped at 10); the firmware renders
  `succ`/`total` directly (clamped defensively to 0..10).

## Errors

- `404 {"error":"No active auto-pi project found."}` — no seed yet.
