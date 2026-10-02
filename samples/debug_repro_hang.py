"""Regression check for the 'white window + not responding on click' failure
of samples/app_graphics_scene.py, without any human interaction.

Background: the sample used to call the blocking ``input()`` right after the
window was created and before ``app.run()`` started the message pump
(``window->poll_events`` only runs inside ``ctx.tick``), so the window sat
unpainted (white) and Windows declared it 'Not Responding' on click.

What it does:
  1. Launches the sample with stdin held open.
  2. Waits for the "py_window" window to appear.
  3. Uses SendMessageTimeoutW(SMTO_ABORTIFHUNG) / IsHungAppWindow to check
     whether the window's thread pumps messages -- the programmatic
     equivalent of 'clicking the window shows Not Responding'.
  4. Reports and cleans up (kills the child process tree).

Exit code: 0 if the hang reproduces (bug present), 1 if the window stays
responsive (bug fixed).

Usage:
  uv run python samples/debug_repro_hang.py -p <project> [-b dx]
"""
import argparse
import ctypes
import subprocess
import sys
import time
from pathlib import Path

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32


def find_window(title_substr: str):
    """Return hwnd of the first visible top-level window whose title contains
    title_substr, or 0."""
    result = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    def enum_proc(hwnd, _lparam):
        if not user32.IsWindowVisible(hwnd):
            return True
        length = user32.GetWindowTextLengthW(hwnd)
        if length == 0:
            return True
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buf, length + 1)
        if title_substr.lower() in buf.value.lower():
            result.append(hwnd)
            return False
        return True

    user32.EnumWindows(enum_proc, 0)
    return result[0] if result else 0


def window_responds(hwnd) -> bool:
    """True if the window thread pumps messages within 2s (SMTO_ABORTIFHUNG)."""
    WM_NULL = 0x0000
    SMTO_ABORTIFHUNG = 0x0002
    res = ctypes.c_ulong()
    # DWORD_PTR SendMessageTimeoutW(HWND, UINT, WPARAM, LPARAM, UINT, UINT, PDWORD_PTR)
    ret = user32.SendMessageTimeoutW(
        hwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 2000, ctypes.byref(res)
    )
    return bool(ret)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-b", "--backend", type=str, default="dx")
    parser.add_argument("-p", "--project", type=str, required=True)
    parser.add_argument("--wait", type=float, default=25.0,
                        help="max seconds to wait for the window")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parent.parent
    cmd = [
        "uv", "run", "python", "samples/app_graphics_scene.py",
        "-p", args.project, "-b", args.backend,
    ]
    print("launching:", " ".join(cmd), flush=True)
    # stdin=PIPE kept open: input() blocks forever, exactly like an idle
    # terminal where the user hasn't pressed Enter yet.
    proc = subprocess.Popen(
        cmd,
        cwd=repo,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        creationflags=subprocess.CREATE_NEW_PROCESS_GROUP,  # type: ignore[attr-defined]
    )

    hwnd = 0
    t0 = time.time()
    while time.time() - t0 < args.wait:
        hwnd = find_window("py_window")
        if hwnd:
            break
        if proc.poll() is not None:
            print("child exited early, rc =", proc.returncode, flush=True)
            out = proc.stdout.read()
            print(out[-3000:])
            sys.exit(1)
        time.sleep(0.5)

    if not hwnd:
        print("FAIL: window 'py_window' never appeared", flush=True)
        proc.kill()
        sys.exit(1)

    print(f"window found (hwnd={hwnd}); checking whether its thread pumps messages",
          flush=True)
    # The main thread is busy with scene/material setup for a few seconds
    # after the window appears; that is startup work, not the bug. Retry for
    # up to ~20s and only declare the bug if the window NEVER pumps.
    responds = False
    hung = True
    for attempt in range(10):
        time.sleep(2.0)
        # re-lookup in case the window was recreated
        hwnd = find_window("py_window") or hwnd
        responds = window_responds(hwnd)
        hung = bool(user32.IsHungAppWindow(hwnd))
        print(f"  attempt {attempt}: responded={responds} hung={hung}", flush=True)
        if responds and not hung:
            break
    print(f"SendMessageTimeout(WM_NULL, 2s) -> responded: {responds}", flush=True)
    print(f"IsHungAppWindow -> {hung}", flush=True)
    if hung or not responds:
        print("REPRODUCED: window never pumps messages (white + "
              "'Not Responding' on click)", flush=True)
        status = "reproduced"
    else:
        print("window responsive: render loop is pumping messages", flush=True)
        status = "responsive"

    # cleanup: kill the whole tree
    subprocess.run(
        ["taskkill", "/F", "/T", "/PID", str(proc.pid)],
        capture_output=True,
    )
    print("child killed. status =", status, flush=True)
    sys.exit(0 if status == "reproduced" else 1)


if __name__ == "__main__":
    main()
