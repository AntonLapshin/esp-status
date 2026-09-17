# /api/esp-status — ESP-optimized endpoint spec

Live implementation: `auto-pi/ui/server/server.js` → `buildEspStatus()`.

Tiny aggregated JSON for ESP32 polling over LAN (~400 bytes, `Cache-Control: no-store`).
Full dashboards should use `/api/status` instead.

## Request

```
GET http://<DEV-LAN-IP>:8787/api/esp-status
```

## Response 200 (v10)

```json
{
  "ok": true,
  "proj": "timeline",
  "loop": true,
  "stuck": false,
  "llmActive": true,
  "persona": "engineer",
  "model": "deepseek-ai/DeepSeek-V4-Flash-0731",
  "lastAction": "pushed feat/foo",
  "lastActionAgoS": 300,
  "last10PersonaStatus": [true, true, false, true],
  "lastPersonaCallFinished": 300,
  "last10LlmStatus": [true, false, true, true, true],
  "lastLlmCallFinished": 47
}
```

| Field | UI element | Source |
|---|---|---|
| `proj` | header: project name | active project name (max 24 chars) |
| `loop` | header: `ON` / `OFF` badge (+ header green/red) | `.pi/state/loop.lock` liveness (stop file forces `false`) |
| `stuck` | `STUCK` banner, large red text | active persona record older than `loop.personaTimeoutMs` (default 1h), silent longer than `loop.personaInactivityMs` (default 10m), or active while the loop is dead |
| `llmActive` | `working…` state (stale bars but child alive) | active run with a live `pi` child |
| `persona` | hero glyph (`PM`, `ENGINEER`, `QA`, `REVIEW`, …) | active persona (started run wins, else last run) |
| `model` | small model line (basename after `/`, max 28 chars) | effective pi model (config → `PI_*` env → pi settings → `health.jsonl`/`llm.jsonl` fallback) |
| `lastAction` | last-action line, e.g. `commit 3m ago` (with `lastActionAgoS`) | newest GitHub-visible event (`issue.*`, `pr.*`, `label.*`, `git.push/commit/merge`); `-` when none yet |
| `lastActionAgoS` | freshness suffix of the last-action line | seconds since `lastAction` (`-1` = never) |
| `last10PersonaStatus` | PERSONA row: up to 9 history bars + white ongoing bar, green=`true` / red=`false`, oldest left, newest history next to white | up to 10 newest `health.jsonl` outcomes (one per whole persona-run invocation + one per retry), newest first on the wire; `[]` when none yet |
| `lastPersonaCallFinished` | `{ago}` line, right after the Persona bars (no "Persona" prefix) | seconds since the newest `health.jsonl` record (success or fail); `-1` when none yet |
| `last10LlmStatus` | LLM row: up to 9 history bars + white ongoing bar, green=`true` / red=`false`, oldest left, newest history next to white | up to 10 newest `llm.jsonl` outcomes (one per finished individual LLM turn), newest first on the wire; `[]` when none yet |
| `lastLlmCallFinished` | `{ago}` line (no "LLM" prefix) | seconds since the newest `llm.jsonl` record (success or fail); `-1` when none yet |

v9 `last10LlmStatus`/`lastLlmCallFinished` meant whole persona runs and were
renamed in v10 to `last10PersonaStatus`/`lastPersonaCallFinished`; the v10
`last10LlmStatus`/`lastLlmCallFinished` are true per-turn LLM calls.
v8 fields (`status`, `state`, `ok_n`/`fail_n`, `run_id`/`run_ok_n`,
`act`/`act_t`/`act_ago_s`, `ago_s`, `last`, `tok_today`, `err`, `at`) were
removed in v9 — old firmware must upgrade.

## Display mapping (firmware v14)

- Header: green = loop on and not stuck, red = stuck or loop off.
  `grey` is ESP-side only (WiFi/HTTP failed).
- Persona colors: PM gold, engineer cyan, QA green, review magenta.
- Bars: both PERSONA and LLM rows render the same way — 10 square slots:
  first 9 are filled green/red history (right-aligned, solid grey on the left
  when fewer than 9), the rightmost slot is always solid white for the ongoing
  call/run.

## Errors

- `404 {"error":"No active auto-pi project found."}` — no seed yet.
