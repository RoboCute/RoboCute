"""Tiny command-line SSE consumer for quick verification of the chat server."""

from __future__ import annotations

import argparse
import json
import sys

import httpx


def stream_chat(base_url: str, message: str) -> None:
    url = f"{base_url}/v1/chat/completions"
    payload = {
        "model": "robocute-chat-demo",
        "messages": [{"role": "user", "content": message}],
        "stream": True,
    }
    with httpx.Client(timeout=60.0) as client:
        with client.stream("POST", url, json=payload, headers={"Accept": "text/event-stream"}) as resp:
            resp.raise_for_status()
            for line in resp.iter_lines():
                if not line.startswith("data: "):
                    continue
                data = line[6:]
                if data == "[DONE]":
                    print("\n[DONE]")
                    break
                try:
                    chunk = json.loads(data)
                    for choice in chunk.get("choices", []):
                        delta = choice.get("delta", {})
                        content = delta.get("content", "")
                        if content:
                            print(content, end="", flush=True)
                except json.JSONDecodeError:
                    print(f"\n[parse error: {data}]", file=sys.stderr)


def main() -> int:
    parser = argparse.ArgumentParser(description="Chat server SSE test client")
    parser.add_argument("--url", default="http://127.0.0.1:8123", help="Server base URL")
    parser.add_argument("message", default="hello", nargs="?", help="User message")
    args = parser.parse_args()

    stream_chat(args.url, args.message)
    return 0


if __name__ == "__main__":
    sys.exit(main())
