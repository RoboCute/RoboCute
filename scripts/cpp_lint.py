"""C++ syntax check CLI using clangd."""

import argparse
import orjson
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, cast


class ClangdLSPClient:
    """Minimal LSP client for clangd to get diagnostics."""

    def __init__(self, clangd_path: str, compile_commands_dir: str):
        self.clangd_path = clangd_path
        self.compile_commands_dir = compile_commands_dir
        self.process: subprocess.Popen[bytes] | None = None
        self.request_id = 0
        self.diagnostics: list[dict[str, Any]] = []

    def start(self) -> None:
        """Start clangd process."""
        cmd = [
            self.clangd_path,
            "--compile-commands-dir=" + self.compile_commands_dir,
            "--log=error",
            "--clang-tidy=true",
            "--completion-style=bundled",
            "--pch-storage=memory",
            "--cross-file-rename=false",
        ]
        self.process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

    def stop(self) -> None:
        """Stop clangd process."""
        if self.process:
            try:
                self._send_request("shutdown", {})
                self._send_notification("exit", {})
                self.process.wait(timeout=2)
            except Exception:
                self.process.kill()
            finally:
                self.process = None

    def _send_message(self, message: bytes) -> None:
        """Send a message to clangd."""
        assert self.process is not None
        assert self.process.stdin is not None
        header = f"Content-Length: {len(message)}\r\n\r\n".encode()
        self.process.stdin.write(header + message)
        self.process.stdin.flush()

    def _send_request(self, method: str, params: dict[str, Any]) -> int:
        """Send a request to clangd."""
        self.request_id += 1
        message = {
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": method,
            "params": params,
        }
        self._send_message(orjson.dumps(message))
        return self.request_id

    def _send_notification(self, method: str, params: dict[str, Any]) -> None:
        """Send a notification to clangd."""
        message = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params,
        }
        self._send_message(orjson.dumps(message))

    def _read_message(self) -> dict[str, Any] | None:
        """Read a message from clangd."""
        if self.process is None:
            return None

        header = b""
        while True:
            assert self.process.stdout is not None
            byte = self.process.stdout.read(1)
            if not byte:
                return None
            header += byte
            if header.endswith(b"\r\n\r\n"):
                break

        content_length = 0
        for line in header.decode().split("\r\n"):
            if line.startswith("Content-Length:"):
                content_length = int(line.split(":")[1].strip())
                break

        if content_length == 0:
            return None

        assert self.process.stdout is not None
        body = self.process.stdout.read(content_length)
        return cast(dict[str, Any], orjson.loads(body))

    def initialize(self) -> None:
        """Initialize the LSP connection."""
        root_uri = Path(self.compile_commands_dir).resolve().as_uri()
        self._send_request(
            "initialize",
            {
                "processId": os.getpid(),
                "rootUri": root_uri,
                "capabilities": {},
                "workspaceFolders": [
                    {"uri": root_uri, "name": Path(self.compile_commands_dir).name}
                ],
            },
        )

        while True:
            msg = self._read_message()
            if msg and "id" in msg and msg.get("result"):
                break

        self._send_notification("initialized", {})

    def open_document(self, file_path: str, content: str) -> None:
        """Open a document in clangd."""
        uri = Path(file_path).resolve().as_uri()
        self._send_notification(
            "textDocument/didOpen",
            {
                "textDocument": {
                    "uri": uri,
                    "languageId": "cpp",
                    "version": 1,
                    "text": content,
                }
            },
        )

    def get_diagnostics(self, file_path: str, timeout: float = 10.0) -> list[dict[str, Any]]:
        """Get diagnostics for a file using textDocument/diagnostic."""
        uri = Path(file_path).resolve().as_uri()
        req_id = self._send_request(
            "textDocument/diagnostic",
            {
                "textDocument": {"uri": uri},
                "identifier": "syntax-check",
            },
        )

        start_time = time.time()
        while time.time() - start_time < timeout:
            msg = self._read_message()
            if msg is None:
                break

            if msg.get("id") == req_id and "result" in msg:
                result = msg["result"]
                if isinstance(result, dict) and "items" in result:
                    return cast(list[dict[str, Any]], result["items"])
                return []

            if msg.get("method") == "textDocument/publishDiagnostics":
                params = cast(dict[str, Any], msg.get("params", {}))
                if params.get("uri") == uri:
                    return cast(list[dict[str, Any]], params.get("diagnostics", []))

        return []


def load_compile_commands(project_root: str = ".") -> str:
    """Find and validate compile_commands.json location."""
    vscode_dir = Path(project_root) / ".vscode"
    compile_commands = vscode_dir / "compile_commands.json"
    if compile_commands.exists():
        return str(vscode_dir)

    build_dir = Path(project_root) / "build"
    compile_commands = build_dir / "compile_commands.json"
    if compile_commands.exists():
        return str(build_dir)

    raise FileNotFoundError(
        "Could not find compile_commands.json in .vscode or build directory"
    )


def format_diagnostic(diag: dict[str, Any]) -> str:
    """Format a diagnostic message."""
    range_info = diag.get("range", {})
    start = range_info.get("start", {})
    line = start.get("line", 0) + 1
    character = start.get("character", 0) + 1

    severity = diag.get("severity", 1)
    severity_str = ["Error", "Error", "Warning", "Info", "Hint"][min(severity, 4)]

    message = diag.get("message", "")
    code = diag.get("code", "")

    result = f"{severity_str}: {message}"
    if code:
        result += f" [{code}]"
    result += f" at line {line}, col {character}"
    return result


def find_clangd(clangd_path: str, project_root: str) -> str:
    """Find clangd executable."""
    if clangd_path == "clangd":
        settings_path = Path(project_root) / ".vscode" / "settings.json"
        if settings_path.exists():
            try:
                with open(settings_path, "r", encoding="utf-8") as f:
                    settings = orjson.loads(f.read())
                config_clangd_path = settings.get("clangd.path")
                if config_clangd_path:
                    resolved_path = Path(project_root) / config_clangd_path
                    if resolved_path.exists():
                        clangd_path = str(resolved_path.resolve())
                    else:
                        config_path = Path(config_clangd_path)
                        if config_path.exists():
                            clangd_path = str(config_path.resolve())
            except (orjson.JSONDecodeError, IOError):
                pass

    if not Path(clangd_path).exists():
        try:
            result = subprocess.run(
                ["where", clangd_path],
                capture_output=True,
                text=True,
            )
            if result.returncode == 0:
                clangd_path = result.stdout.strip().split("\n")[0].strip()
        except Exception:
            pass

    return clangd_path


def run_lint(file_path: str, project_root: str, clangd_path: str, verbose: bool) -> int:
    """Run C++ lint and return exit code."""
    file_path_obj = Path(file_path).resolve()
    if not file_path_obj.exists():
        print(f"Error: File not found: {file_path_obj}", file=sys.stderr)
        return 2

    file_args = ""
    try:
        compile_commands_dir = load_compile_commands(project_root)
    except FileNotFoundError:
        compile_commands_dir = project_root
    else:
        compile_commands_path = Path(compile_commands_dir) / "compile_commands.json"
        if not compile_commands_path.exists():
            if verbose:
                print(f"Compile arguments:\n{file_args}", file=sys.stderr)
            print("Error: compile_commands.json not found.", file=sys.stderr)
            return 2

        try:
            with open(compile_commands_path, "r", encoding="utf-8") as f:
                compile_commands = cast(list[dict[str, Any]], orjson.loads(f.read()))

            file_maps: dict[str, str] = {}
            for entry in compile_commands:
                file_value = entry.get("file")
                if file_value is None:
                    continue
                name = str(file_value).replace("\\", "/").replace("//", "/")
                args = entry.get("arguments", [])
                args_str = "\n".join(args)
                if not name:
                    continue
                file_maps[name] = args_str

            rel_path = str(file_path_obj.relative_to(Path(project_root).resolve()))
            file_path_str = str(rel_path).replace("\\", "/").replace("//", "/")
            file_args = file_maps.get(file_path_str) or ""
            if not file_args:
                try:
                    file_args = file_maps.get(rel_path) or ""
                except ValueError:
                    pass
                if not file_args:
                    print(
                        f"Error: File not found in compile_commands.json: {file_path}",
                        file=sys.stderr,
                    )
                    return 2
        except (orjson.JSONDecodeError, IOError) as e:
            if verbose:
                print(f"Compile arguments:\n{file_args}\nError: {e}", file=sys.stderr)
            print(f"Error: compile_commands.json decode error: {e}", file=sys.stderr)
            return 2

    clangd_path = find_clangd(clangd_path, project_root)
    if not Path(clangd_path).exists():
        if verbose:
            print(f"Compile arguments:\n{file_args}", file=sys.stderr)
        print(f"Error: clangd not found: {clangd_path}", file=sys.stderr)
        return 2

    try:
        content = file_path_obj.read_text(encoding="utf-8")
    except Exception as e:
        if verbose:
            print(f"Compile arguments:\n{file_args}", file=sys.stderr)
        print(f"Error: Failed to read file: {e}", file=sys.stderr)
        return 2

    client = ClangdLSPClient(clangd_path, compile_commands_dir)
    try:
        client.start()
        client.initialize()
        client.open_document(str(file_path_obj), content)
        time.sleep(0.5)
        diagnostics = client.get_diagnostics(str(file_path_obj))

        if not diagnostics:
            output = "No issues found!"
            if verbose:
                output += f"\nCompile arguments:\n{file_args}"
            print(output)
            return 0

        errors = 0
        warnings = 0
        formatted = []
        for diag in diagnostics:
            formatted.append(format_diagnostic(diag))
            severity = diag.get("severity", 1)
            if severity <= 1:
                errors += 1
            elif severity == 2:
                warnings += 1

        summary = f"\n{'-' * 60}\nTotal: {errors} error(s), {warnings} warning(s)"
        output = "\n".join(formatted) + summary
        if verbose:
            output += f"\nCompile arguments:\n{file_args}"
        print(output)

        if errors > 0:
            return 1
        return 0
    except Exception as e:
        output = ""
        if verbose:
            output = f"Compile arguments:\n{file_args}"
        if output:
            print(output, file=sys.stderr)
        print(f"Error: Failed to check C++ syntax: {e}", file=sys.stderr)
        return 2
    finally:
        client.stop()


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate C++ file syntax using clangd.")
    parser.add_argument("file_path", help="Path to the C++ file to validate.")
    parser.add_argument(
        "--project-root",
        default=".",
        help="Root directory of the project (default: current directory).",
    )
    parser.add_argument(
        "--clangd-path",
        default="clangd",
        help="Path to the clangd executable (default: 'clangd').",
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Include verbose compilation arguments in output.",
    )
    args = parser.parse_args()

    code = run_lint(args.file_path, args.project_root, args.clangd_path, args.verbose)
    sys.exit(code)


if __name__ == "__main__":
    main()
