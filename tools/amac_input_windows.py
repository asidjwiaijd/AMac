"""Windows capture backend, installed only for an explicit foreground session."""
import ctypes
from ctypes import wintypes
import os


class WindowsCapture:
    def __init__(self, hwnd, on_key, on_exit):
        if os.name != "nt":
            raise RuntimeError("当前桌面控制台支持 Windows。")
        self.user32 = ctypes.WinDLL("user32", use_last_error=True)
        self.kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self.on_key = on_key
        self.on_exit = on_exit
        self.hook = None
        self.active = False
        self.down = set()
        self.center = (0, 0)
        self.restore_position = None
        self._declare_api()
        self.hwnd = self.user32.GetAncestor(hwnd, 2) or hwnd

    def _declare_api(self):
        u = self.user32
        u.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
        u.GetAncestor.restype = wintypes.HWND
        u.GetForegroundWindow.restype = wintypes.HWND
        u.GetCursorPos.argtypes = [ctypes.POINTER(wintypes.POINT)]
        u.GetCursorPos.restype = wintypes.BOOL
        u.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
        u.SetCursorPos.restype = wintypes.BOOL
        u.ClipCursor.argtypes = [ctypes.POINTER(wintypes.RECT)]
        u.ClipCursor.restype = wintypes.BOOL
        u.SetCapture.argtypes = [wintypes.HWND]
        u.SetCapture.restype = wintypes.HWND
        u.ReleaseCapture.restype = wintypes.BOOL
        u.GetAsyncKeyState.argtypes = [ctypes.c_int]
        u.GetAsyncKeyState.restype = ctypes.c_short
        self.HOOKPROC = ctypes.WINFUNCTYPE(ctypes.c_ssize_t, ctypes.c_int,
                                         wintypes.WPARAM, wintypes.LPARAM)
        u.SetWindowsHookExW.argtypes = [ctypes.c_int, self.HOOKPROC, wintypes.HINSTANCE, wintypes.DWORD]
        u.SetWindowsHookExW.restype = wintypes.HANDLE
        u.UnhookWindowsHookEx.argtypes = [wintypes.HANDLE]
        u.UnhookWindowsHookEx.restype = wintypes.BOOL
        u.CallNextHookEx.argtypes = [wintypes.HANDLE, ctypes.c_int, wintypes.WPARAM, wintypes.LPARAM]
        u.CallNextHookEx.restype = ctypes.c_ssize_t
        self.kernel32.GetModuleHandleW.argtypes = [wintypes.LPCWSTR]
        self.kernel32.GetModuleHandleW.restype = wintypes.HMODULE

        class KeyData(ctypes.Structure):
            _fields_ = [("vkCode", wintypes.DWORD), ("scanCode", wintypes.DWORD),
                        ("flags", wintypes.DWORD), ("time", wintypes.DWORD),
                        ("dwExtraInfo", ctypes.c_size_t)]
        self.KeyData = KeyData

    def foreground(self):
        return self.user32.GetForegroundWindow() == self.hwnd

    def position(self):
        point = wintypes.POINT()
        if not self.user32.GetCursorPos(ctypes.byref(point)):
            raise ctypes.WinError(ctypes.get_last_error())
        return point.x, point.y

    @staticmethod
    def normalize_key(vk, scan, flags):
        if vk == 0x10:
            return 0xA1 if scan == 0x36 else 0xA0
        if vk == 0x11:
            return 0xA3 if flags & 1 else 0xA2
        if vk == 0x12:
            return 0xA5 if flags & 1 else 0xA4
        return vk

    def _keyboard(self, code, message, address):
        try:
            if code >= 0 and self.active and self.foreground():
                data = ctypes.cast(address, ctypes.POINTER(self.KeyData)).contents
                vk = self.normalize_key(data.vkCode, data.scanCode, data.flags)
                pressed = message in (0x100, 0x104)
                released = message in (0x101, 0x105)
                if pressed or released:
                    if pressed:
                        self.down.add(vk)
                    else:
                        self.down.discard(vk)
                    ctrl = bool(self.down.intersection((0xA2, 0xA3, 0x11)))
                    alt = bool(self.down.intersection((0xA4, 0xA5, 0x12)))
                    if pressed and vk == 0x1B and ctrl and alt:
                        self.on_exit("已退出键鼠控制。")
                        return 1
                    if pressed and vk == 0x09 and alt:
                        # Alt itself is allowed through below, so Windows
                        # can perform Alt+Tab while the Mac keys are released.
                        self.on_exit("已切换窗口并释放键鼠控制。")
                        return self.user32.CallNextHookEx(self.hook, code, message, address)
                    handled = self.on_key(vk, pressed)
                    if handled and vk not in (0x12, 0xA4, 0xA5):
                        return 1
        except Exception:
            # Never let an exception strand a system-wide keyboard hook.
            self.on_exit("捕获已停止，请重新开始。")
        return self.user32.CallNextHookEx(self.hook, code, message, address)

    def start(self, rect, capture_hwnd):
        if self.active:
            return
        if not self.foreground():
            raise RuntimeError("请先点击控制台窗口，再开始控制。")
        self.restore_position = self.position()
        self.center = ((rect[0] + rect[2]) // 2, (rect[1] + rect[3]) // 2)
        self.down = {key for key in (0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5)
                     if self.user32.GetAsyncKeyState(key) & 0x8000}
        self.callback = self.HOOKPROC(self._keyboard)
        try:
            self.hook = self.user32.SetWindowsHookExW(13, self.callback,
                self.kernel32.GetModuleHandleW(None), 0)
            if not self.hook:
                raise ctypes.WinError(ctypes.get_last_error())
            bounds = wintypes.RECT(*rect)
            if not self.user32.ClipCursor(ctypes.byref(bounds)):
                raise ctypes.WinError(ctypes.get_last_error())
            self.user32.SetCapture(capture_hwnd)
            if not self.user32.SetCursorPos(*self.center):
                raise ctypes.WinError(ctypes.get_last_error())
            self.active = True
        except Exception:
            self.stop()
            raise

    def poll(self):
        if not self.active or not self.foreground():
            return None
        x, y = self.position()
        dx, dy = x - self.center[0], y - self.center[1]
        if dx or dy:
            if not self.user32.SetCursorPos(*self.center):
                raise ctypes.WinError(ctypes.get_last_error())
        return dx, dy

    def stop(self, restore=True):
        was_active = self.active
        self.active = False
        if self.hook:
            self.user32.UnhookWindowsHookEx(self.hook)
            self.hook = None
        self.user32.ClipCursor(None)
        self.user32.ReleaseCapture()
        if was_active and restore and self.restore_position:
            self.user32.SetCursorPos(*self.restore_position)
        self.down.clear()


def enable_dpi_awareness():
    if os.name == "nt":
        try:
            ctypes.windll.user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
        except (AttributeError, OSError):
            pass
