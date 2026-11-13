"""Input edge ordering, held-key ownership and safe USB worker teardown."""
from pathlib import Path
import queue
import sys
import threading
import time
import types
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from amac_input import CaptureSession, InputBuffer, InputEvent, UsbInputWorker, event_command
from amac_input_windows import WindowsCapture


def drain(buffer):
    result = []
    while (event := buffer.get(0)) is not None:
        result.append((event.kind, *event.values))
    return result


class RoutingTests(unittest.TestCase):
    def setUp(self):
        self.buffer = InputBuffer()
        self.session = CaptureSession(self.buffer)

    def start(self, **kwargs):
        self.session.begin(**kwargs)
        self.assertEqual(drain(self.buffer), [("release",)])

    def test_only_explicit_capture_accepts_input(self):
        self.session.key(65, True)
        self.session.motion(10, 10)
        self.assertEqual(drain(self.buffer), [])
        self.start()
        self.session.key(65, True)
        self.session.end()
        self.session.key(66, True)
        self.assertEqual(drain(self.buffer), [("release",)])

    def test_two_modifier_aliases_and_autorepeat_release_once(self):
        self.start()
        self.session.key(0xA0, True)
        self.session.key(0xA0, True) # OS repeat must not add another claim.
        self.session.key(0xA1, True)
        self.session.key(0xA0, False)
        self.session.key(0xA1, False)
        self.assertEqual(drain(self.buffer), [("key", 0x38, True), ("key", 0x38, False)])

    def test_motion_coalesces_without_crossing_click_edges(self):
        self.start()
        for _ in range(100):
            self.session.motion(2, -1)
        self.session.button(0, True)
        self.session.motion(5, 6)
        self.session.button(0, False)
        self.assertEqual(drain(self.buffer), [
            ("move", 200, -100), ("button", 0, True), ("move", 5, 6), ("button", 0, False)])

    def test_slow_motion_preserves_fraction_and_negative_sign(self):
        self.start()
        for _ in range(4):
            self.session.motion(1, -1, 0.25)
        self.assertEqual(drain(self.buffer), [("move", 1, -1)])

    def test_right_click_does_not_release_physical_ctrl_or_left_button(self):
        self.start()
        self.session.key(0xA2, True)
        self.session.button(0, True)
        self.session.button(1, True)
        self.session.button(0, False)
        self.session.button(1, False)
        self.session.key(0xA2, False)
        self.assertEqual(drain(self.buffer), [
            ("key", 0x36, True), ("button", 0, True),
            ("button", 0, False), ("key", 0x36, False)])

    def test_optional_ctrl_command_and_win_share_claims(self):
        self.start(ctrl_command=True)
        for vk in (0xA2, 0x5B):
            self.session.key(vk, True)
        self.session.key(0xA2, False)
        self.session.key(0x5B, False)
        self.assertEqual(drain(self.buffer), [("key", 0x37, True), ("key", 0x37, False)])

    def test_overflow_discards_old_actions_and_releases_everything(self):
        buffer = InputBuffer(capacity=3)
        session = CaptureSession(buffer)
        session.begin()
        session.key(65, True)
        session.key(66, True)
        session.key(67, True)
        self.assertFalse(session.active)
        self.assertFalse(buffer.active)
        self.assertEqual(drain(buffer), [("release",)])

    def test_wheel_keeps_an_already_held_arrow_pressed(self):
        self.start()
        self.session.key(0x26, True)
        self.session.wheel(3)
        self.session.key(0x26, False)
        self.assertEqual(drain(self.buffer), [("key", 0x3E, True), ("key", 0x3E, False)])

    def test_protocol_and_left_right_modifier_normalization(self):
        self.assertEqual(event_command(InputEvent("key", (0x37, True), 0)), ("KEY DOWN 55", "OK KEY DOWN"))
        self.assertEqual(event_command(InputEvent("move", (-12, 7), 0)), ("MOUSE REL -12 7", "OK MOUSE REL"))
        self.assertEqual(WindowsCapture.normalize_key(0x10, 0x36, 0), 0xA1)
        self.assertEqual(WindowsCapture.normalize_key(0x11, 0x1D, 1), 0xA3)
        self.assertEqual(WindowsCapture.normalize_key(0x12, 0x38, 0), 0xA4)


class WorkerTests(unittest.TestCase):
    def make_worker(self, failure=None):
        self.messages = queue.Queue()
        self.buffer = InputBuffer()
        self.client = None
        self.factory_calls = []
        outer = self

        class Client:
            protocol_version = 5
            def __init__(self, port, reset_on_failure):
                outer.factory_calls.append((port, reset_on_failure))
                self.serial = types.SimpleNamespace(write_timeout=5)
                self.commands = []
                self.closed = False
                self.got_key = threading.Event()
                outer.client = self
            def connect(self, timeout):
                self.connect_timeout = timeout
            def request(self, command, expected, timeout):
                self.commands.append(command)
                if command.startswith("KEY"):
                    self.got_key.set()
                    if failure:
                        raise RuntimeError(failure)
                if command == "BOOT STATUS":
                    return "@B2 OK BOOT STATUS emulator none 0"
                return "@B2 " + expected
            def close(self):
                self.closed = True
        worker = UsbInputWorker("COM12", self.buffer, self.messages, client_factory=Client)
        return worker

    def wait_for(self, kind):
        while True:
            message = self.messages.get(timeout=2)
            if message[0] == kind:
                return message

    def test_attach_never_resets_and_disconnect_releases_held_key(self):
        worker = self.make_worker()
        worker.start()
        self.wait_for("phase")
        session = CaptureSession(self.buffer)
        session.begin()
        session.key(65, True)
        self.assertTrue(self.client.got_key.wait(2))
        worker.stop()
        worker.join(2)
        self.assertFalse(worker.is_alive())
        self.assertEqual(self.factory_calls, [("COM12", False)])
        self.assertTrue(self.client.closed)
        self.assertEqual(self.client.commands[-1], "RELEASE_ALL")
        self.assertNotIn("KEY UP 0", self.client.commands) # One final release covers disconnect.

    def test_transport_failure_stops_capture_and_attempts_release(self):
        worker = self.make_worker("session_changed")
        worker.start()
        self.wait_for("phase")
        session = CaptureSession(self.buffer)
        session.begin()
        session.key(65, True)
        self.assertIn("session_changed", self.wait_for("error")[1]["message"])
        worker.join(2)
        self.assertFalse(self.buffer.active)
        self.assertTrue(self.client.closed)
        self.assertEqual(self.client.commands[-1], "RELEASE_ALL")

    def test_delayed_input_is_dropped_instead_of_replayed(self):
        worker = self.make_worker()
        worker.start()
        self.wait_for("phase")
        with self.buffer.condition:
            self.buffer.active = True
            self.buffer.events.append(InputEvent("key", (0, True), time.monotonic()-2))
            self.buffer.condition.notify_all()
        self.wait_for("paused")
        worker.stop()
        worker.join(2)
        self.assertFalse(any(c.startswith("KEY") for c in self.client.commands))


if __name__ == "__main__":
    unittest.main()
