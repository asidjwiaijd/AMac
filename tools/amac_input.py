"""Input routing and the single-owner USB worker for the aMac desktop app.

This module has no GUI or global input hooks. Only explicit capture sessions
accept input. The existing tagged protocol handles retry without replaying a
click or a relative movement.
"""
from collections import Counter, deque
from dataclasses import dataclass
import queue
import threading
import time

from mac_control import ADB_KEYS, MacControl

APP_VERSION = "0.1.0"

# Windows virtual keys, translated to the same ADB convention as mac_control.
VK_TO_ADB = {ord(c.upper()): ADB_KEYS[c] for c in "abcdefghijklmnopqrstuvwxyz0123456789"}
VK_TO_ADB.update({
    0x08: 0x33, 0x09: 0x30, 0x0D: 0x24, 0x1B: 0x35, 0x20: 0x31,
    0x10: 0x38, 0xA0: 0x38, 0xA1: 0x38,
    0x11: 0x36, 0xA2: 0x36, 0xA3: 0x36,
    0x12: 0x3A, 0xA4: 0x3A, 0xA5: 0x3A,
    0x5B: 0x37, 0x5C: 0x37, 0x14: 0x39,
    0x25: 0x3B, 0x27: 0x3C, 0x28: 0x3D, 0x26: 0x3E,
    0x21: 0x74, 0x22: 0x79, 0x23: 0x77, 0x24: 0x73, 0x2E: 0x75,
    0xBA: 0x29, 0xBB: 0x18, 0xBC: 0x2B, 0xBD: 0x1B, 0xBE: 0x2F,
    0xBF: 0x2C, 0xC0: 0x32, 0xDB: 0x21, 0xDC: 0x2A, 0xDD: 0x1E, 0xDE: 0x27,
    0x60: 0x52, 0x61: 0x53, 0x62: 0x54, 0x63: 0x55, 0x64: 0x56,
    0x65: 0x57, 0x66: 0x58, 0x67: 0x59, 0x68: 0x5B, 0x69: 0x5C,
    0x6A: 0x43, 0x6B: 0x45, 0x6D: 0x4E, 0x6E: 0x41, 0x6F: 0x4B,
})
VK_TO_ADB.update(dict(zip(range(0x70, 0x7C),
    (0x7A, 0x78, 0x63, 0x76, 0x60, 0x61, 0x62, 0x64, 0x65, 0x6D, 0x67, 0x6F))))


@dataclass
class InputEvent:
    kind: str
    values: tuple
    created: float


class InputBuffer:
    """Bounded ordered input; adjacent motion coalesces, edges never do."""
    def __init__(self, capacity=256, clock=time.monotonic):
        self.capacity = capacity
        self.clock = clock
        self.condition = threading.Condition()
        self.events = deque()
        self.active = False
        self.closed = False

    def begin(self):
        with self.condition:
            self.events.clear()
            self.active = not self.closed
            if self.active:
                self.events.append(InputEvent("release", (), self.clock()))
            self.condition.notify_all()

    def release(self):
        with self.condition:
            self.active = False
            self.events.clear()
            if not self.closed:
                self.events.append(InputEvent("release", (), self.clock()))
            self.condition.notify_all()

    def close(self):
        with self.condition:
            self.active = False
            self.closed = True
            self.events.clear()
            self.condition.notify_all()

    def put(self, kind, *values):
        with self.condition:
            if not self.active or self.closed:
                return False
            if kind == "move" and self.events and self.events[-1].kind == "move":
                last = self.events[-1]
                last.values = (last.values[0] + values[0], last.values[1] + values[1])
            elif len(self.events) >= self.capacity:
                self.active = False
                self.events.clear()
                self.events.append(InputEvent("release", (), self.clock()))
                self.condition.notify_all()
                return False
            else:
                self.events.append(InputEvent(kind, tuple(values), self.clock()))
            self.condition.notify_all()
            return True

    def get(self, timeout=0.05):
        with self.condition:
            if not self.events and not self.closed:
                self.condition.wait(timeout)
            return self.events.popleft() if self.events else None


class CaptureSession:
    """Own key/button claims so aliases and Ctrl+right-click cannot stick."""
    def __init__(self, buffer):
        self.buffer = buffer
        self.active = False
        self.keys = {}
        self.key_claims = Counter()
        self.buttons = set()
        self.button_claims = Counter()
        self.remainder = [0.0, 0.0]
        self.ctrl_command = False

    def begin(self, ctrl_command=False):
        self.end()
        self.ctrl_command = ctrl_command
        self.buffer.begin()
        self.active = self.buffer.active

    def end(self):
        self.active = False
        self.keys.clear()
        self.key_claims.clear()
        self.buttons.clear()
        self.button_claims.clear()
        self.remainder = [0.0, 0.0]
        self.buffer.release()

    def _send(self, kind, *values):
        if not self.buffer.put(kind, *values):
            self.active = False
            return False
        return True

    def _key(self, code, down):
        previous = self.key_claims[code]
        self.key_claims[code] = max(0, previous + (1 if down else -1))
        if (previous == 0) != (self.key_claims[code] == 0):
            self._send("key", code, bool(self.key_claims[code]))

    def _button(self, button, down):
        previous = self.button_claims[button]
        self.button_claims[button] = max(0, previous + (1 if down else -1))
        if (previous == 0) != (self.button_claims[button] == 0):
            self._send("button", button, bool(self.button_claims[button]))

    def key(self, vk, down):
        if not self.active:
            return False
        code = VK_TO_ADB.get(vk)
        if code is None:
            return False
        if self.ctrl_command and vk in (0x11, 0xA2, 0xA3):
            code = 0x37
        if down and vk not in self.keys:
            self.keys[vk] = code
            self._key(code, True)
        elif not down and vk in self.keys:
            self._key(self.keys.pop(vk), False)
        return True

    def motion(self, dx, dy, sensitivity=1.0):
        if not self.active:
            return
        self.remainder[0] += dx * sensitivity
        self.remainder[1] += dy * sensitivity
        x, y = int(self.remainder[0]), int(self.remainder[1])
        self.remainder[0] -= x
        self.remainder[1] -= y
        if x or y:
            self._send("move", x, y)

    def button(self, button, down):
        if not self.active or (button in self.buttons) == down:
            return
        if down:
            self.buttons.add(button)
        else:
            self.buttons.remove(button)
        if button == 1: # Match physical mouse behavior: Ctrl+left-click.
            if down:
                self._key(0x36, True)
                self._button(0, True)
            else:
                self._button(0, False)
                self._key(0x36, False)
        elif button in (0, 2):
            self._button(button, down)

    def wheel(self, notches):
        if not self.active:
            return
        code = 0x3E if notches > 0 else 0x3D
        for _ in range(min(abs(notches), 12)):
            self._key(code, True)
            self._key(code, False)


def event_command(event):
    if event.kind == "release":
        return "RELEASE_ALL", "OK RELEASE_ALL"
    if event.kind == "move":
        x, y = event.values
        return f"MOUSE REL {x} {y}", "OK MOUSE REL"
    if event.kind == "key":
        code, down = event.values
        action = "DOWN" if down else "UP"
        return f"KEY {action} {code}", f"OK KEY {action}"
    if event.kind == "button":
        button, down = event.values
        action = "DOWN" if down else "UP"
        return f"MOUSE {action} {button}", f"OK MOUSE {action}"
    raise ValueError("Unknown input event")


def available_ports():
    from serial.tools import list_ports
    return sorted(list_ports.comports(), key=lambda p: (p.vid != 0x303A, p.device))


class UsbInputWorker(threading.Thread):
    """All serial operations live on one thread, away from input callbacks."""
    def __init__(self, port, buffer, notifications, client_factory=MacControl, ports_factory=available_ports):
        super().__init__(name="aMac USB input", daemon=True)
        self.port = port
        self.buffer = buffer
        self.notifications = notifications
        self.client_factory = client_factory
        self.ports_factory = ports_factory
        self.stop_requested = threading.Event()
        self.client = None
        self.sent = 0

    def notify(self, kind, **values):
        self.notifications.put((kind, values))

    def stop(self):
        self.buffer.close()
        self.stop_requested.set()

    def _connect(self):
        ports = [self.port] if self.port else [p.device for p in self.ports_factory()
            if (p.vid, p.pid) in ((0x303A, 0x1001), (0x1A86, 0x55D3))]
        if not ports:
            raise RuntimeError("未发现设备，请连接 USB2 数据线并保持 USB1 供电。")
        last = None
        for port in ports:
            if self.stop_requested.is_set():
                return False
            client = None
            try:
                client = self.client_factory(port, reset_on_failure=False)
                client.serial.write_timeout = 1.0
                client.connect(timeout=2.5)
                if client.protocol_version < 4:
                    raise RuntimeError("设备控制协议较旧，请先更新固件。")
                self.client = client
                self.notify("connected", port=port)
                return True
            except Exception as error:
                last = error
                if client:
                    client.close()
        raise RuntimeError("无法连接设备，请关闭占用串口的维护工具后重试。") from last

    def run(self):
        try:
            if not self._connect():
                return
            last_heartbeat = last_phase = last_stats = 0.0
            while not self.stop_requested.is_set():
                now = time.monotonic()
                event = self.buffer.get(0.02)
                if event:
                    # Do not replay delayed actions into a later guest state.
                    if event.kind != "release" and time.monotonic() - event.created > 0.5:
                        self.buffer.release()
                        self.notify("paused", message="输入等待过久，已停止控制，请重新开始。")
                        continue
                    command, expected = event_command(event)
                    self.client.request(command, expected, timeout=1.2)
                    self.sent += 1
                now = time.monotonic()
                if now - last_phase >= 3:
                    line = self.client.request("BOOT STATUS", "OK BOOT STATUS", timeout=1.2)
                    phase = line.split()[4]
                    self.notify("phase", phase=phase)
                    if phase != "emulator" and self.buffer.active:
                        self.buffer.release()
                        self.notify("paused", message="Mac 已停止运行，键鼠控制已释放。")
                    last_phase = now
                if now - last_heartbeat >= 1:
                    self.client.request("PING", "OK PONG", timeout=1.2)
                    last_heartbeat = now
                if now - last_stats >= 1:
                    self.notify("stats", sent=self.sent)
                    last_stats = now
        except Exception as error:
            self.buffer.release()
            self.notify("error", message=str(error))
        finally:
            if self.client:
                try:
                    self.client.request("RELEASE_ALL", "OK RELEASE_ALL", timeout=0.5)
                except Exception:
                    pass
                self.client.close()
            self.notify("disconnected")
