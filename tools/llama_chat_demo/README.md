# RoboCute Chat Demo Server

A tiny FastAPI server that provides an OpenAI-compatible `/v1/chat/completions`
endpoint for the RoboCute Qt chat demo.

## Modes

### Mock mode (default)

Runs without any external model and returns a deterministic streaming reply so
that the frontend can be verified immediately.

```powershell
uv run tools\llama_chat_demo\chat_server.py --port 8123
```

Test with curl:

```powershell
curl -N -X POST http://127.0.0.1:8123/v1/chat/completions `
  -H "Content-Type: application/json" `
  -d '{"model":"robocute-chat-demo","messages":[{"role":"user","content":"hello"}],"stream":true}'
```

### llama-server mode

If both `LLAMA_SERVER_PATH` and `LLAMA_CHAT_MODEL_PATH` are configured, the
demo spawns a local `llama-server` child process and proxies chat requests to it.

```powershell
$env:LLAMA_SERVER_PATH="C:\path\to\llama-server.exe"
$env:LLAMA_CHAT_MODEL_PATH="C:\path\to\model.gguf"
uv run tools\llama_chat_demo\chat_server.py --port 8123
```

Extra flags may be passed through `LLAMA_SERVER_EXTRA_ARGS` (shell-quoted) or
`--llama-extra-args`. Managed flags such as `-m`, `--port`, `--host`, and
`--parallel` are rejected because the demo sets them itself.

## Configuration

| Variable | CLI flag | Description |
|----------|----------|-------------|
| `LLAMA_CHAT_HOST` | `--host` | Bind host (default `127.0.0.1`) |
| `LLAMA_CHAT_PORT` | `--port` | Bind port (default `8123`) |
| `LLAMA_CHAT_MOCK` | `--mock` | Force mock mode |
| `LLAMA_SERVER_PATH` | `--llama-server-path` | Path to `llama-server` executable |
| `LLAMA_CHAT_MODEL_PATH` | `--llama-model-path` | Path to `.gguf` model |
| `LLAMA_SERVER_EXTRA_ARGS` | `--llama-extra-args` | Extra shell-quoted flags |
| `LLAMA_SERVER_STARTUP_TIMEOUT` | `--llama-startup-timeout` | Seconds to wait for `/health` |

## Files

- `chat_server.py` — FastAPI application and OpenAI-compatible endpoints.
- `server_config.py` — Configuration parsing and CLI flag deny-list.
- `llama_process.py` — Spawns and stops the `llama-server` child process.
- `test_client.py` — Minimal SSE consumer for quick command-line tests.
