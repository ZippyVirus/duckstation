"""Windows native UI smoke test. Uses only isolated, newly launched processes."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request
import zipfile

user32 = ctypes.windll.user32
callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
http = urllib.request.build_opener(urllib.request.ProxyHandler({}))

def titles(pid):
    result = []
    @callback_type
    def visit(hwnd, unused):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid:
            buf = ctypes.create_unicode_buffer(2048)
            user32.GetWindowTextW(hwnd, buf, len(buf))
            if buf.value:
                result.append(buf.value)
        return True
    user32.EnumWindows(visit, 0)
    return result

def wait_for(predicate, label):
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        try:
            value = predicate()
            if value:
                return value
        except (OSError, ValueError):
            pass
        time.sleep(0.2)
    raise AssertionError("Timed out: " + label)

class Client:
    def __init__(self, port, name):
        self.url = f"http://127.0.0.1:{port}/mcp"
        self.session = None
        self.seq = 0
        self.request("initialize", {"protocolVersion":"2025-11-25",
                     "capabilities":{}, "clientInfo":{"name":name,"version":"1"}})
        self.request("notifications/initialized", notification=True)

    def request(self, method, params=None, notification=False):
        payload = {"jsonrpc":"2.0","method":method}
        if params is not None:
            payload["params"] = params
        if not notification:
            self.seq += 1
            payload["id"] = self.seq
        headers = {"Content-Type":"application/json",
                   "Accept":"application/json, text/event-stream",
                   "MCP-Protocol-Version":"2025-11-25"}
        if self.session:
            headers["MCP-Session-Id"] = self.session
        req = urllib.request.Request(self.url, json.dumps(payload).encode(), headers)
        with http.open(req, timeout=5) as response:
            self.session = response.headers.get("MCP-Session-Id", self.session)
            raw = response.read().decode()
        if notification or not raw:
            return None
        if raw.startswith("event:") or raw.startswith("data:"):
            raw = next(line[5:].strip() for line in raw.splitlines() if line.startswith("data:"))
        reply = json.loads(raw)
        if "error" in reply:
            raise AssertionError(reply)
        return reply.get("result", {})

    def identity(self, **args):
        r = self.request("tools/call", {"name":"mcp_identity", "arguments":args})
        assert not r.get("isError"), r
        return r

    def close(self):
        headers = {"MCP-Session-Id":self.session, "MCP-Protocol-Version":"2025-11-25"}
        with http.open(urllib.request.Request(self.url, headers=headers, method="DELETE"), timeout=5):
            pass

processes = []
report = []
with tempfile.TemporaryDirectory(prefix="mcp-ui-") as scratch:
    try:
        for label, task in [("Codex", "identity test A"), ("Claude", "identity test B")]:
            root = Path(scratch) / label
            with zipfile.ZipFile(sys.argv[1]) as archive:
                archive.extractall(root)
            exe = next(root.rglob("duckstation-qt-x64-ReleaseLTCG.exe"))
            runtime = exe.parent
            (runtime / "portable.txt").touch()
            with socket.socket() as probe:
                probe.bind(("127.0.0.1", 0))
                port = probe.getsockname()[1]
            (runtime / "settings.ini").write_text(
                "[Main]\nSetupWizardIncomplete = false\n"
                "[Debug]\nEnableMCPServer = true\nMCPServerPort = " + str(port) + "\n",
                encoding="utf-8")
            env = dict(os.environ, DUCKSTATION_MCP_OWNER=label, DUCKSTATION_MCP_TASK=task)
            proc = subprocess.Popen([str(exe)], cwd=runtime, env=env)
            processes.append(proc)
            client = wait_for(lambda: Client(port, label.lower()), "server " + label)
            expected = f"MCP :{port} | Owner: {label} / {task}"
            wait_for(lambda: any(expected in t for t in titles(proc.pid)), "initial title")
            initial = titles(proc.pid)
            client.identity(action="set", owner=label, task="reassigned")
            wait_for(lambda: any(f"Owner: {label} / reassigned" in t for t in titles(proc.pid)), "reassigned title")
            client.close()
            wait_for(lambda: any("reassigned" in t and "Client:" not in t for t in titles(proc.pid)), "DELETE title")
            client = Client(port, "reconnected")
            wait_for(lambda: any("Client: reconnected" in t for t in titles(proc.pid)), "reconnect title")
            client.identity(action="clear")
            wait_for(lambda: any("Owner: unassigned" in t for t in titles(proc.pid)), "clear title")
            final = titles(proc.pid)
            assert all(t.count("[MCP :") <= 1 for t in final), final
            client.close()
            report.append({"pid":proc.pid,"port":port,"owner":label,"initial":initial,"final":final})
        assert len({item["port"] for item in report}) == 2
        print(json.dumps(report, indent=2))
        Path("native-ui-smoke.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    finally:
        for proc in processes:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=15)
