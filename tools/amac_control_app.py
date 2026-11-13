"""Windows desktop keyboard/mouse controller for the physical aMac device."""
import argparse
import os
from pathlib import Path
import queue
import sys
import time
import tkinter as tk
from tkinter import ttk, messagebox

from amac_input import APP_VERSION, CaptureSession, InputBuffer, UsbInputWorker, available_ports
from amac_input_windows import WindowsCapture, enable_dpi_awareness


class ControlApp:
    def __init__(self, root):
        self.root = root
        self.worker = None
        self.buffer = None
        self.session = None
        self.native = None
        self.connected = False
        self.phase = ""
        self.closing = False
        self.notifications = queue.Queue()
        self.port_map = {}
        self.wheel_remainder = 0
        self.status = tk.StringVar(value="先连接 USB2 数据线，再点击连接设备。")
        self.port = tk.StringVar(value="自动识别 USB2")
        self.counter = tk.StringVar(value="尚未连接")
        self.sensitivity = tk.DoubleVar(value=1.0)
        self.ctrl_command = tk.BooleanVar(value=False)
        self._build_ui()
        self.refresh_ports()
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.root.bind("<FocusOut>", lambda _e: self.root.after(50, self._check_focus))
        self.root.bind("<Configure>", self._configured)
        self.root.after(20, self._messages)
        self.root.after(8, self._mouse_tick)

    def _build_ui(self):
        r = self.root
        r.title(f"aMac 键鼠控制台 {APP_VERSION}")
        r.geometry("840x620")
        r.minsize(740, 560)
        r.configure(bg="#f5f6fa")
        assets = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parents[1])) / "assets"
        icon = assets / "happymacIcon.png"
        if icon.exists():
            self.icon = tk.PhotoImage(file=str(icon))
            r.iconphoto(True, self.icon)
        style = ttk.Style(r)
        style.theme_use("clam")
        style.configure("TFrame", background="#f5f6fa")
        style.configure("TLabel", background="#f5f6fa", foreground="#243247", font=("Microsoft YaHei UI", 10))
        style.configure("TButton", font=("Microsoft YaHei UI", 10), padding=(12, 8))
        style.configure("TCheckbutton", background="#f5f6fa", font=("Microsoft YaHei UI", 10))
        top = ttk.Frame(r, padding=24)
        top.pack(fill="both", expand=True)
        ttk.Label(top, text="把电脑的键鼠，交给 aMac", font=("Microsoft YaHei UI", 23, "bold")).pack(anchor="w")
        ttk.Label(top, text="通过 USB2 发送输入 · 控制时请看设备屏幕", foreground="#64748b").pack(anchor="w", pady=(5, 16))
        row = ttk.Frame(top)
        row.pack(fill="x")
        self.ports = ttk.Combobox(row, textvariable=self.port, state="readonly", width=35,
                                  font=("Microsoft YaHei UI", 10))
        self.ports.pack(side="left", fill="x", expand=True)
        self.refresh_button = ttk.Button(row, text="刷新", command=self.refresh_ports)
        self.refresh_button.pack(side="left", padx=8)
        self.connect_button = ttk.Button(row, text="连接设备", command=self.toggle_connection)
        self.connect_button.pack(side="left")
        ttk.Label(top, textvariable=self.status, wraplength=750).pack(anchor="w", pady=(12, 12))
        self.pad = tk.Canvas(top, bg="#e9edf6", highlightthickness=1,
                             highlightbackground="#d4dce8", height=238, takefocus=True)
        self.pad.pack(fill="both", expand=True)
        self.pad.bind("<Configure>", lambda _e: self._draw_pad())
        self.pad.bind("<ButtonPress-1>", lambda _e: self._button(0, True))
        self.pad.bind("<ButtonRelease-1>", lambda _e: self._button(0, False))
        self.pad.bind("<ButtonPress-3>", lambda _e: self._button(1, True))
        self.pad.bind("<ButtonRelease-3>", lambda _e: self._button(1, False))
        self.pad.bind("<ButtonPress-2>", lambda _e: self._button(2, True))
        self.pad.bind("<ButtonRelease-2>", lambda _e: self._button(2, False))
        self.pad.bind("<MouseWheel>", self._wheel)
        settings = ttk.Frame(top)
        settings.pack(fill="x", pady=(14, 8))
        ttk.Label(settings, text="鼠标速度").pack(side="left")
        self.speed = ttk.Scale(settings, from_=0.25, to=2.0, variable=self.sensitivity, length=180)
        self.speed.pack(side="left", padx=(10, 20))
        self.ctrl_check = ttk.Checkbutton(settings, variable=self.ctrl_command, text="将 Ctrl 当作 Mac 的 Command")
        self.ctrl_check.pack(side="left")
        actions = ttk.Frame(top)
        actions.pack(fill="x", pady=(8, 4))
        self.start_button = ttk.Button(actions, text="开始控制", command=self.begin, state="disabled")
        self.start_button.pack(side="left")
        self.stop_button = ttk.Button(actions, text="释放控制", command=self.end, state="disabled")
        self.stop_button.pack(side="left", padx=8)
        ttk.Label(actions, text="退出：Ctrl + Alt + Esc", font=("Microsoft YaHei UI", 11, "bold")).pack(side="right")
        ttk.Label(top, text="Win = Command  ·  Alt = Option  ·  Alt+Tab 自动释放后切换窗口",
                  foreground="#64748b").pack(anchor="w", pady=(7, 0))
        ttk.Label(top, textvariable=self.counter, foreground="#64748b").pack(anchor="w", pady=(5, 0))

    def _draw_pad(self):
        p = self.pad
        p.delete("all")
        w, h = p.winfo_width(), p.winfo_height()
        active = bool(self.session and self.session.active)
        p.configure(bg="#dcebdc" if active else "#e9edf6")
        p.create_text(w/2, h/2-35, text="正在控制 aMac" if active else "准备好后，点击「开始控制」",
                      fill="#243247", font=("Microsoft YaHei UI", 19, "bold"))
        p.create_text(w/2, h/2+9, text="鼠标已捕获，键盘输入发送到设备" if active else "设备进入 Mac 桌面后再开始",
                      fill="#536477", font=("Microsoft YaHei UI", 11))
        p.create_text(w/2, h/2+46, text="Ctrl + Alt + Esc 随时退出",
                      fill="#536477", font=("Microsoft YaHei UI", 12))

    def refresh_ports(self):
        self.port_map = {"自动识别 USB2": None}
        try:
            for p in available_ports():
                self.port_map[f"{p.device}  {p.description}"] = p.device
        except Exception as error:
            self.status.set(f"读取设备列表失败：{error}")
        self.ports.configure(values=list(self.port_map))
        if self.port.get() not in self.port_map:
            self.port.set("自动识别 USB2")

    def toggle_connection(self):
        if self.worker:
            self.disconnect()
            return
        self.buffer = InputBuffer()
        self.session = CaptureSession(self.buffer)
        self.worker = UsbInputWorker(self.port_map.get(self.port.get()), self.buffer, self.notifications)
        self.status.set("正在连接，设备会继续正常运行…")
        self.connect_button.configure(text="取消连接")
        self.ports.configure(state="disabled")
        self.refresh_button.configure(state="disabled")
        self.worker.start()

    def _set_controls(self):
        active = bool(self.session and self.session.active)
        self.start_button.configure(state="normal" if self.connected and self.phase == "emulator" and not active else "disabled")
        self.stop_button.configure(state="normal" if active else "disabled")
        self.speed.configure(state="disabled" if active else "normal")
        self.ctrl_check.configure(state="disabled" if active else "normal")

    def begin(self):
        if not self.connected or self.phase != "emulator" or not self.session:
            return
        if os.name != "nt":
            messagebox.showinfo("运行环境", "当前桌面控制台支持 Windows。")
            return
        self.pad.focus_set()
        self.root.update_idletasks()
        self.session.begin(self.ctrl_command.get())
        try:
            self.native = WindowsCapture(self.root.winfo_id(), self._key, self._exit_from_hook)
            x, y = self.pad.winfo_rootx(), self.pad.winfo_rooty()
            self.native.start((x+3, y+3, x+self.pad.winfo_width()-3, y+self.pad.winfo_height()-3), self.pad.winfo_id())
            self.pad.configure(cursor="none")
            self.wheel_remainder = 0
            self.status.set("控制中。按 Ctrl + Alt + Esc 退出；Alt+Tab 会释放控制。")
        except Exception as error:
            self.end()
            self.status.set(f"无法开始控制：{error}")
        self._draw_pad()
        self._set_controls()

    def _exit_from_hook(self, reason):
        if self.session:
            self.session.end()
        # The hook callback must return promptly; GUI/unhook work is deferred.
        self.root.after(0, lambda: self.end(reason))

    def end(self, reason="已释放键鼠，可以继续使用电脑。", restore=True):
        if self.native:
            self.native.stop(restore=restore)
            self.native = None
        if self.session:
            self.session.end()
        self.pad.configure(cursor="")
        self.status.set(reason)
        self._draw_pad()
        self._set_controls()

    def _key(self, vk, down):
        if not self.session or not self.session.active:
            return False
        result = self.session.key(vk, down)
        if not self.session.active:
            self._exit_from_hook("输入过多，已安全释放，请重新开始。")
        return result

    def _button(self, button, down):
        if self.session and self.session.active:
            self.session.button(button, down)

    def _wheel(self, event):
        if self.session and self.session.active:
            self.wheel_remainder += event.delta
            notches = int(self.wheel_remainder / 120)
            self.wheel_remainder -= notches * 120
            self.session.wheel(notches)
            return "break"

    def _mouse_tick(self):
        try:
            if self.native and self.session and self.session.active:
                motion = self.native.poll()
                if motion is None:
                    self.end("窗口已失去焦点，键鼠已释放。", restore=False)
                else:
                    self.session.motion(*motion, sensitivity=self.sensitivity.get())
                    if not self.session.active:
                        self.end("输入过多，已释放控制，请重新开始。")
        except Exception as error:
            self.end(f"捕获已停止：{error}")
        if not self.closing:
            self.root.after(8, self._mouse_tick)

    def _check_focus(self):
        if self.native and not self.native.foreground():
            self.end("窗口已失去焦点，键鼠已释放。", restore=False)

    def _configured(self, event):
        if event.widget == self.root and self.native:
            self.end("窗口大小或位置已改变，键鼠已释放。", restore=False)

    def _messages(self):
        try:
            while True:
                kind, data = self.notifications.get_nowait()
                if kind == "connected":
                    self.connected = True
                    self.connect_button.configure(text="断开连接")
                    self.status.set(f"已连接 {data['port']}，正在确认 Mac 状态。")
                elif kind == "phase":
                    self.phase = data["phase"]
                    if not (self.session and self.session.active):
                        self.status.set("设备已连接。看到 Mac 桌面后可点击开始控制。" if self.phase == "emulator"
                                        else "设备已连接，请先在设备上启动 Mac。")
                elif kind == "stats":
                    self.counter.set(f"已发送 {data['sent']} 条输入指令  ·  本机软件 {APP_VERSION}")
                elif kind == "paused":
                    self.end(data["message"])
                elif kind == "error":
                    self.end(f"连接中断：{data['message']}")
                elif kind == "disconnected":
                    self.end(self.status.get())
                    self.connected = False
                    self.phase = ""
                    self.worker = None
                    self.connect_button.configure(text="连接设备")
                    self.ports.configure(state="readonly")
                    self.refresh_button.configure(state="normal")
                self._set_controls()
        except queue.Empty:
            pass
        if not self.closing:
            self.root.after(20, self._messages)

    def disconnect(self):
        self.end("正在断开设备…")
        if self.worker:
            self.worker.stop()
        self.connected = False
        self._set_controls()

    def close(self):
        self.closing = True
        self.disconnect()
        deadline = time.monotonic() + 2.5
        def finish():
            if self.worker and self.worker.is_alive() and time.monotonic() < deadline:
                self.root.after(50, finish)
            else:
                self.root.destroy()
        finish()


def main():
    parser = argparse.ArgumentParser(description="aMac Windows 键鼠控制台")
    parser.add_argument("--smoke-test", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    enable_dpi_awareness()
    root = tk.Tk()
    if args.smoke_test:
        root.withdraw()
    app = ControlApp(root)
    if args.smoke_test:
        root.after(700, app.close)
    root.mainloop()


if __name__ == "__main__":
    main()
