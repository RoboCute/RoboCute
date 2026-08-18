"""Configuration for the RoboCute llama-server chat demo.

Supports both mock mode (deterministic replies) and llama-server mode, where a
local llama-server binary is spawned and proxied through OpenAI-compatible
endpoints.
"""

from __future__ import annotations

import argparse
import os
import shlex
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Optional


# Flags that this demo manages itself; user-supplied extra args cannot set them.
_MANAGED_LLAMA_FLAGS: frozenset[str] = frozenset(
    {
        "-m",
        "--model",
        "--port",
        "--host",
        "--parallel",
        "--n-parallel",
        "-np",
        "-a",
        "--alias",
        "--api-key",
        "--api-key-file",
        "--ssl-key-file",
        "--ssl-cert-file",
        "--log-file",
        "--log-disable",
        "-h",
        "--help",
        "--version",
    }
)


@dataclass
class ServerConfig:
    """Runtime configuration for the chat server."""

    host: str = "127.0.0.1"
    port: int = 8123
    mock: bool = True

    # llama-server mode settings
    llama_server_path: Optional[Path] = None
    llama_model_path: Optional[Path] = None
    llama_extra_args: list[str] = field(default_factory=list)
    llama_startup_timeout: float = 120.0
    llama_request_timeout: float = 600.0

    # Request proxy limits
    max_request_size_bytes: int = 2 * 1024 * 1024  # 2 MiB

    def upstream_base_url(self, port: int) -> str:
        """Return the upstream llama-server URL for the given port."""
        return f"http://127.0.0.1:{port}"


def _flag_name(token: str) -> Optional[str]:
    """Return the normalised flag name for a CLI token, or None if not a flag."""
    token = token.strip()
    if not token.startswith("-") or token in {"-", "--"}:
        return None
    if len(token) >= 2 and (token[1].isdigit() or token[1] == "."):
        return None
    name = token.split("=", 1)[0]
    if name.startswith("--"):
        name = name.replace("_", "-")
    return name


def validate_llama_extra_args(args: Optional[Iterable[str]]) -> list[str]:
    """Validate user-supplied llama-server extra flags.

    Raises:
        ValueError: If a managed flag or bare positional value is encountered.
    """
    if not args:
        return []

    out: list[str] = []
    pending_values = 0
    for raw in args:
        token = str(raw)
        flag = _flag_name(token)
        if flag is not None:
            if flag in _MANAGED_LLAMA_FLAGS:
                raise ValueError(
                    f"llama-server flag '{flag}' is managed by the demo server "
                    "and cannot be passed as an extra arg"
                )
            # Most flags consume the next token as their value. Switches do not.
            # This simple parser treats all non-switch long flags as value-taking.
            if "=" not in token and not flag.startswith("--no-") and len(flag) > 2:
                pending_values += 1
            out.append(token)
        else:
            if pending_values <= 0:
                raise ValueError(
                    f"extra llama-server args cannot contain a bare value ('{token[:64]}')"
                )
            pending_values -= 1
            out.append(token)

    return out


def parse_config(argv: Optional[list[str]] = None) -> ServerConfig:
    """Parse command-line arguments and environment variables into a config."""
    parser = argparse.ArgumentParser(
        description="RoboCute chat demo server (mock or llama-server proxy)."
    )
    parser.add_argument(
        "--host",
        default=os.environ.get("LLAMA_CHAT_HOST", "127.0.0.1"),
        help="Host to bind the demo server (default: 127.0.0.1).",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=int(os.environ.get("LLAMA_CHAT_PORT", "8123")),
        help="Port to bind the demo server (default: 8123).",
    )
    parser.add_argument(
        "--mock",
        action="store_true",
        default=os.environ.get("LLAMA_CHAT_MOCK", "").lower() in {"1", "true", "yes"},
        help="Force mock mode even if LLAMA_SERVER_PATH is set.",
    )
    parser.add_argument(
        "--llama-server-path",
        type=Path,
        default=os.environ.get("LLAMA_SERVER_PATH"),
        help="Path to the llama-server executable.",
    )
    parser.add_argument(
        "--llama-model-path",
        type=Path,
        default=os.environ.get("LLAMA_CHAT_MODEL_PATH"),
        help="Path to the .gguf model file.",
    )
    parser.add_argument(
        "--llama-extra-args",
        type=str,
        default=os.environ.get("LLAMA_SERVER_EXTRA_ARGS", ""),
        help="Additional llama-server flags (shell-quoted string).",
    )
    parser.add_argument(
        "--llama-startup-timeout",
        type=float,
        default=float(os.environ.get("LLAMA_SERVER_STARTUP_TIMEOUT", "120.0")),
        help="Seconds to wait for llama-server to become healthy.",
    )

    args = parser.parse_args(argv)

    extra_args_text: str = args.llama_extra_args
    try:
        extra_args_list = shlex.split(extra_args_text)
    except ValueError as exc:
        raise ValueError(f"failed to parse --llama-extra-args: {exc}") from exc

    validated_extra_args = validate_llama_extra_args(extra_args_list)

    config = ServerConfig(
        host=args.host,
        port=args.port,
        mock=args.mock,
        llama_server_path=args.llama_server_path,
        llama_model_path=args.llama_model_path,
        llama_extra_args=validated_extra_args,
        llama_startup_timeout=args.llama_startup_timeout,
    )

    # Decide mock mode automatically when no model is configured.
    has_server = config.llama_server_path is not None and config.llama_server_path.exists()
    has_model = config.llama_model_path is not None and config.llama_model_path.exists()
    if not config.mock and not (has_server and has_model):
        if not config.mock:
            print(
                "[chat_server] LLAMA_SERVER_PATH or LLAMA_CHAT_MODEL_PATH not set; "
                "falling back to mock mode.",
                file=sys.stderr,
            )
            config.mock = True

    return config
