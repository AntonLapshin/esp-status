# /api/esp-status — ESP-optimized endpoint spec

Live implementation: `auto-pi/ui/server/server.js` → `buildEspStatus()`.

Tiny aggregated JSON for ESP32 polling over LAN (~250 bytes, `Cache-Control: no-store`).
Full dashboards should use `/api/status` instead.

## Request

```
GET http://<DEV-LAN-IP>:8787/api/esp-status
```

## Response 200 (v9)

```json
{
  "ok": true,
  "proj": "timeline",
  "loop": true,
  "stuck": false,
  "persona": "engineer",
  "model": "deepseek-ai/DeepSeek-V4-Flash-0731",
  "lastAction": "pushed feat/foo",
  "lastActionAgoS": 300,
  "last10LlmStatus": [true, true, false, true],
  "lastLlmCallFinished": 47
}
```

| Field | UI element | Source |
|---|---|---|
| `proj` | header: project name | active project name (max 24 chars) |
| `loop` | header: `ON` / `OFF` badge (+ header green/red) | `.pi/state/loop.lock` liveness (stop file forces `false`) |
| `stuck` | `STUCK` banner, large red text | active persona record older than `loop.personaTimeoutMs` (default 1h), silent longer than `loop.personaInactivityMs` (default 10m), or active while the loop is dead |
| `persona` | hero glyph (`PM`, `ENGINEER`, `QA`, `REVIEW`, …) | active persona (started run wins, else last run) |
| `model` | small model line (basename after `/`, max 28 chars) | effective pi model (config → `PI_*` env → pi settings → `health.jsonl` fallback) |
| `lastAction` | last-action line, e.g. `commit 3m ago` (with `lastActionAgoS`) | newest GitHub-visible event (`issue.*`, `pr.*`, `git.push/commit/merge`); `-` when none yet |
| `lastActionAgoS` | freshness suffix of the last-action line | seconds since `lastAction` (`-1` = never) |
| `last10LlmStatus` | up to 10 bars, green=`true` / red=`false`, oldest left, newest right | up to 10 newest `health.jsonl` outcomes (success or fail), newest first on the wire; `[]` when none yet |
| `lastLlmCallFinished` | `last llm call 5m ago` line | seconds since the newest `health.jsonl` record (success or fail); `-1` when none yet |

v8 fields (`status`, `state`, `ok_n`/`fail_n`, `run_id`/`run_ok_n`,
`act`/`act_t`/`act_ago_s`, `ago_s`, `last`, `tok_today`, `err`, `at`) were
removed in v9 — old firmware must upgrade.

## Display mapping (firmware)

- Header: green = loop on and not stuck, red = stuck or loop off.
  `grey` is ESP-side only (WiFi/HTTP failed).
- Persona colors: PM gold, engineer cyan, QA green, review magenta.
- Bars: filled green/red for recorded calls, dim outline for empty slots;
  newest bar gets a white top edge.

## Errors

- `404 {"error":"No active auto-pi project found."}` — no seed yet.
