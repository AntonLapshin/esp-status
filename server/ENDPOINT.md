# /api/esp-status — ESP-optimized endpoint spec

Live implementation: `auto-pi/ui/server/server.js` → `buildEspStatus()`.

Tiny aggregated JSON for ESP32 polling over LAN (~260 bytes, `Cache-Control: no-store`).
Full dashboards should use `/api/status` instead.

## Request

```
GET http://<DEV-LAN-IP>:8787/api/esp-status
```

## Response 200 (v3)

```json
{
  "ok": true,
  "proj": "timeline",
  "loop": true,
  "status": "green",
  "provider": "joingonka",
  "succ": 74,
  "total": 99,
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
| `succ`, `total` | `SUCCESS` gauge (last 10 calls, e.g. `9/10 90%`) | successful / total LLM calls from `health.jsonl` (cumulative; firmware folds deltas into a rolling last-10 window) |
| `persona` | `PERSONA` hero glyph (`PM`, `ENGINEER`, `QA`, `REVIEW`, …) | active persona (started run wins, else last run) |
| `ago_s` | freshness (`47s ago`, footer) | seconds since last run/event (`-1` = never) |

Legacy fields (`last`, `runs`, `ok_n`, `fail_n`, `tok_today`, `err`) are still
sent so pre-v3 firmware keeps working, but v3 firmware no longer displays them.

## Display mapping (firmware)

- Dot: `green` = healthy loop, `red` = stopped / stale / recent error.
  `grey` is ESP-side only (WiFi/HTTP failed).
- Persona colors: PM gold, engineer cyan, QA green, review magenta.
- Gauge: green ≥ 90 %, yellow ≥ 60 %, red below. Firmware shows the rolling
  last 10 provider calls (inferred from `succ`/`total` deltas between polls).

## Errors

- `404 {"error":"No active auto-pi project found."}` — no seed yet.
