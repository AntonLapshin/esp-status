# /api/esp-status — ESP-optimized endpoint spec

Live implementation: `auto-pi/ui/server/server.js` → `buildEspStatus()`.

Tiny aggregated JSON for ESP32 polling over LAN (~240 bytes, `Cache-Control: no-store`).
Full dashboards should use `/api/status` instead.

## Request

```
GET http://<DEV-LAN-IP>:8787/api/esp-status
```

## Response 200

```json
{
  "ok": true,
  "status": "green",
  "loop": true,
  "persona": "engineer",
  "last": "dispatch review: PR #52 ready for review",
  "ago_s": 47,
  "runs": 12,
  "ok_n": 10,
  "fail_n": 1,
  "tok_today": 184500,
  "err": 0,
  "proj": "timeline",
  "at": "2026-09-13T18:32:38.206Z"
}
```

| Field | Source |
|---|---|
| `status` | green: loop on + `ago_s` ≤ 300; yellow: ≤ 900; red: loop off / recent error / stale / no data |
| `loop` | `.pi/state/loop.lock` liveness |
| `persona`, `last` | last `runs.jsonl` record (`persona`, `reason` truncated to 40 chars, fallback: latest event type) |
| `ago_s` | seconds since last run/event (`-1` = never) |
| `runs`, `ok_n`, `fail_n` | today's runs from `runs.jsonl` |
| `tok_today` | today's tokens from `usage.jsonl` |
| `err` | total `errors.jsonl` records |
| `proj` | active project name (max 24 chars) |

## Errors

- `404 {"error":"No active auto-pi project found."}` — no seed yet.
