"""RoboCute chat demo server.

Exposes a minimal OpenAI-compatible HTTP API:
  - GET  /health
  - GET  /v1/models
  - POST /v1/chat/completions

The server can run in mock mode (default) or wrap a local llama-server binary
when LLAMA_SERVER_PATH and LLAMA_CHAT_MODEL_PATH are configured.
"""

from __future__ import annotations

import asyncio
import json
import os
import sys
import time
from contextlib import asynccontextmanager
from typing import Any, AsyncGenerator, Optional

import httpx
from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, StreamingResponse
from pydantic import BaseModel, Field
from uvicorn import Config, Server

from server_config import ServerConfig, parse_config
from llama_process import LlamaProcess, LlamaProcessError


# ============================================================================
# OpenAI-compatible request/response models
# ============================================================================


class ChatMessage(BaseModel):
    role: str
    content: str


class ChatCompletionRequest(BaseModel):
    model: str = "robocute-chat-demo"
    messages: list[ChatMessage]
    stream: bool = True
    max_tokens: int = Field(default=256, ge=1, le=8192)
    temperature: float = Field(default=0.7, ge=0.0, le=2.0)


# ============================================================================
# Server state
# ============================================================================


class ChatServerState:
    """Mutable server state shared across request handlers."""

    def __init__(self, config: ServerConfig):
        self.config = config
        self.llama: Optional[LlamaProcess] = None
        self.mock_client: Optional[httpx.AsyncClient] = None

    async def init(self) -> None:
        if self.config.mock:
            self.mock_client = httpx.AsyncClient()
            return

        self.llama = LlamaProcess(
            server_path=self.config.llama_server_path,  # type: ignore[arg-type]
            model_path=self.config.llama_model_path,  # type: ignore[arg-type]
            host="127.0.0.1",
            extra_args=self.config.llama_extra_args,
            startup_timeout=self.config.llama_startup_timeout,
        )
        self.llama.start()

    async def shutdown(self) -> None:
        if self.mock_client is not None:
            await self.mock_client.aclose()
            self.mock_client = None
        if self.llama is not None:
            self.llama.stop()
            self.llama = None


state: ChatServerState


# ============================================================================
# Helpers
# ============================================================================


def _last_user_message(messages: list[ChatMessage]) -> str:
    for msg in reversed(messages):
        if msg.role == "user":
            return msg.content
    return ""


def _mock_stream(
    request: ChatCompletionRequest,
) -> AsyncGenerator[str, None]:
    """Yield a deterministic mock SSE stream."""
    last_message = _last_user_message(request.messages)
    reply = (
        "Hello! This is the RoboCute chat demo. "
        f'You said: "{last_message}".'
    )
    model = request.model or "robocute-chat-demo"
    created = int(time.time())
    id_ = "chatcmpl-demo"

    # Emit the first chunk with the assistant role.
    chunk = {
        "id": id_,
        "object": "chat.completion.chunk",
        "created": created,
        "model": model,
        "choices": [
            {
                "index": 0,
                "delta": {"role": "assistant", "content": ""},
                "finish_reason": None,
            }
        ],
    }
    yield f"data: {json.dumps(chunk)}\n\n"

    # Emit content word-by-word.
    for word in reply.split(" "):
        chunk["choices"][0]["delta"] = {"content": word + " "}
        yield f"data: {json.dumps(chunk)}\n\n"

    # Final chunk.
    chunk["choices"][0]["delta"] = {}
    chunk["choices"][0]["finish_reason"] = "stop"
    yield f"data: {json.dumps(chunk)}\n\n"
    yield "data: [DONE]\n\n"


def _mock_non_stream(request: ChatCompletionRequest) -> dict[str, Any]:
    last_message = _last_user_message(request.messages)
    content = (
        "Hello! This is the RoboCute chat demo. "
        f'You said: "{last_message}".'
    )
    created = int(time.time())
    return {
        "id": "chatcmpl-demo",
        "object": "chat.completion",
        "created": created,
        "model": request.model,
        "choices": [
            {
                "index": 0,
                "message": {"role": "assistant", "content": content},
                "finish_reason": "stop",
            }
        ],
        "usage": {
            "prompt_tokens": sum(len(m.content.split()) for m in request.messages),
            "completion_tokens": len(content.split()),
            "total_tokens": 0,
        },
    }


def _merge_request_body(request: ChatCompletionRequest) -> dict[str, Any]:
    """Convert the validated request into a plain dict for upstream forwarding."""
    return {
        "model": request.model,
        "messages": [{"role": m.role, "content": m.content} for m in request.messages],
        "stream": request.stream,
        "max_tokens": request.max_tokens,
        "temperature": request.temperature,
    }


# ============================================================================
# App factory
# ============================================================================


def create_app(config: ServerConfig) -> FastAPI:
    global state  # noqa: PLW0603

    @asynccontextmanager
    async def lifespan(app: FastAPI):
        global state  # noqa: PLW0603
        state = ChatServerState(config)
        await state.init()
        yield
        await state.shutdown()

    app = FastAPI(title="RoboCute Chat Demo", version="0.1.0", lifespan=lifespan)

    app.add_middleware(
        CORSMiddleware,
        allow_origins=[
            "http://localhost",
            "http://127.0.0.1",
            f"http://localhost:{config.port}",
            f"http://127.0.0.1:{config.port}",
        ],
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    # ------------------------------------------------------------------
    # Health / model list
    # ------------------------------------------------------------------

    @app.get("/health")
    async def health():
        if config.mock:
            return {"status": "ok", "mode": "mock"}
        if state.llama is None or state.llama.client is None:
            raise HTTPException(status_code=503, detail="llama-server not ready")
        try:
            resp = await state.llama.client.get("/health")
            return {
                "status": "ok" if resp.status_code == 200 else "degraded",
                "mode": "llama-server",
                "upstream_status": resp.status_code,
            }
        except httpx.RequestError as exc:
            raise HTTPException(
                status_code=503, detail=f"upstream unreachable: {exc}"
            ) from exc

    @app.get("/v1/models")
    async def list_models():
        if config.mock:
            return {
                "object": "list",
                "data": [
                    {
                        "id": "robocute-chat-demo",
                        "object": "model",
                        "created": int(time.time()),
                        "owned_by": "robocute",
                    }
                ],
            }
        if state.llama is None or state.llama.client is None:
            raise HTTPException(status_code=503, detail="llama-server not ready")
        resp = await state.llama.client.get("/v1/models")
        return resp.json()

    # ------------------------------------------------------------------
    # Chat completions
    # ------------------------------------------------------------------

    @app.post("/v1/chat/completions")
    async def chat_completions(request: Request, body: ChatCompletionRequest):
        if body.stream:
            return StreamingResponse(
                _chat_stream(body, request),
                media_type="text/event-stream",
                headers={"Cache-Control": "no-cache"},
            )
        return JSONResponse(content=await _chat_non_stream(body))

    async def _chat_non_stream(body: ChatCompletionRequest) -> dict[str, Any]:
        if config.mock:
            return _mock_non_stream(body)

        if state.llama is None or state.llama.client is None:
            raise HTTPException(status_code=503, detail="llama-server not ready")

        try:
            resp = await state.llama.client.post(
                "/v1/chat/completions",
                json=_merge_request_body(body),
                timeout=config.llama_request_timeout,
            )
            resp.raise_for_status()
            return resp.json()
        except httpx.HTTPStatusError as exc:
            raise HTTPException(
                status_code=exc.response.status_code,
                detail=exc.response.text,
            ) from exc
        except httpx.RequestError as exc:
            raise HTTPException(
                status_code=502, detail=f"upstream request failed: {exc}"
            ) from exc

    async def _chat_stream(
        body: ChatCompletionRequest,
        request: Request,
    ) -> AsyncGenerator[str, None]:
        if config.mock:
            async for chunk in _mock_stream_async(body, request):
                yield chunk
            return

        if state.llama is None or state.llama.client is None:
            raise HTTPException(status_code=503, detail="llama-server not ready")

        upstream_request = state.llama.client.build_request(
            "POST",
            "/v1/chat/completions",
            json=_merge_request_body(body),
            headers={"Accept": "text/event-stream"},
        )
        try:
            async with state.llama.client.stream(
                "POST",
                upstream_request.url,
                content=upstream_request.content,
                headers=upstream_request.headers,
                timeout=config.llama_request_timeout,
            ) as upstream:
                async for line in upstream.aiter_lines():
                    if await request.is_disconnected():
                        await upstream.aclose()
                        break
                    yield line + "\n"
        except httpx.RequestError as exc:
            raise HTTPException(
                status_code=502, detail=f"upstream request failed: {exc}"
            ) from exc

    return app


async def _mock_stream_async(
    request: ChatCompletionRequest,
    request_obj: Request,
) -> AsyncGenerator[str, None]:
    """Wrap the synchronous mock generator with disconnect checks."""
    for chunk in _mock_stream(request):
        if await request_obj.is_disconnected():
            break
        yield chunk


# ============================================================================
# Entry point
# ============================================================================


def main() -> int:
    config = parse_config()
    app = create_app(config)

    print(
        f"[chat_server] Starting RoboCute chat demo on {config.host}:{config.port} "
        f"({'mock' if config.mock else 'llama-server'} mode)",
        flush=True,
    )

    cfg = Config(
        app=app,
        host=config.host,
        port=config.port,
        log_level="info",
    )
    server = Server(cfg)
    try:
        server.run()
    except (KeyboardInterrupt, SystemExit):
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
