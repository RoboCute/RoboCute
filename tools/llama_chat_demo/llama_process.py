"""Helpers for spawning and managing a local llama-server subprocess."""

from __future__ import annotations

import asyncio
import atexit
import os
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

import httpx


class LlamaProcessError(RuntimeError):
    """Raised when the llama-server subprocess cannot be started or reached."""


class LlamaProcess:
    """Manages a llama-server child process and its upstream HTTP client."""

    def __init__(
        self,
        server_path: Path,
        model_path: Path,
        host: str = "127.0.0.1",
        port: Optional[int] = None,
        extra_args: Optional[list[str]] = None,
        startup_timeout: float = 120.0,
    ):
        self.server_path = server_path
        self.model_path = model_path
        self.host = host
        self.port = port or _find_free_port(host)
        self.extra_args = list(extra_args or [])
        self.startup_timeout = startup_timeout
        self.proc: Optional[subprocess.Popen] = None
        self.client: Optional[httpx.AsyncClient] = None
        self.stdout_lines: list[str] = []
        self.stderr_lines: list[str] = []

    @property
    def upstream_url(self) -> str:
        return f"http://{self.host}:{self.port}"

    def _log(self, message: str) -> None:
        print(f"[llama_process] {message}", flush=True)

    def _build_command(self) -> list[str]:
        return [
            str(self.server_path),
            "-m",
            str(self.model_path),
            "--port",
            str(self.port),
            "--host",
            self.host,
            "-np",
            "1",
            *self.extra_args,
        ]

    def start(self) -> None:
        """Spawn the llama-server subprocess and wait for /health."""
        if self.proc is not None:
            return

        command = self._build_command()
        self._log(f"starting: {' '.join(command)}")

        creationflags = 0
        if sys.platform == "win32":
            creationflags = subprocess.CREATE_NEW_PROCESS_GROUP

        try:
            self.proc = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                errors="replace",
                creationflags=creationflags,
            )
        except OSError as exc:
            raise LlamaProcessError(f"failed to start llama-server: {exc}") from exc

        # Register cleanup on normal interpreter exit.
        atexit.register(self.stop)

        # Capture the first few lines of output for diagnostics.
        self._start_output_reader()

        # Wait for the server to answer /health.
        self._wait_for_health()
        self.client = httpx.AsyncClient(
            base_url=self.upstream_url,
            timeout=httpx.Timeout(600.0, connect=30.0),
        )
        self._log("llama-server is healthy")

    def _start_output_reader(self) -> None:
        """Start background threads that collect startup stdout/stderr."""
        if self.proc is None:
            return

        def read_stream(stream, buffer: list[str], name: str) -> None:
            try:
                for line in stream:
                    buffer.append(line.rstrip("\n"))
                    if len(buffer) > 200:
                        buffer.pop(0)
                    print(f"[llama_server:{name}] {line.rstrip()}", flush=True)
            except Exception as exc:  # noqa: BLE001
                print(f"[llama_process] {name} reader exited: {exc}", flush=True)

        import threading

        if self.proc.stdout:
            threading.Thread(
                target=read_stream,
                args=(self.proc.stdout, self.stdout_lines, "stdout"),
                daemon=True,
            ).start()
        if self.proc.stderr:
            threading.Thread(
                target=read_stream,
                args=(self.proc.stderr, self.stderr_lines, "stderr"),
                daemon=True,
            ).start()

    def _wait_for_health(self) -> None:
        if self.proc is None:
            raise LlamaProcessError("process not started")

        deadline = time.monotonic() + self.startup_timeout
        url = f"{self.upstream_url}/health"

        with httpx.Client(timeout=httpx.Timeout(5.0, connect=2.0)) as client:
            while time.monotonic() < deadline:
                if self.proc.poll() is not None:
                    rc = self.proc.returncode
                    raise LlamaProcessError(
                        f"llama-server exited early with code {rc}; "
                        f"stderr: {' | '.join(self.stderr_lines[-10:])}"
                    )
                try:
                    resp = client.get(url)
                    if resp.status_code == 200:
                        return
                except httpx.RequestError:
                    pass
                time.sleep(0.5)

        raise LlamaProcessError(
            f"llama-server did not become healthy within {self.startup_timeout}s"
        )

    def stop(self) -> None:
        """Terminate the llama-server subprocess and close the HTTP client."""
        if self.client is not None:
            try:
                asyncio.get_event_loop().run_until_complete(self.client.aclose())
            except Exception:  # noqa: BLE001
                pass
            self.client = None

        if self.proc is None:
            return

        self._log("stopping llama-server")
        try:
            if sys.platform == "win32":
                self.proc.terminate()
            else:
                self.proc.terminate()

            try:
                self.proc.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                self._log("llama-server did not terminate gracefully, killing")
                self.proc.kill()
                self.proc.wait(timeout=5.0)
        except Exception as exc:  # noqa: BLE001
            self._log(f"error while stopping llama-server: {exc}")
        finally:
            self.proc = None


def _find_free_port(host: str = "127.0.0.1") -> int:
    """Return an available TCP port on the given host."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        return int(sock.getsockname()[1])
