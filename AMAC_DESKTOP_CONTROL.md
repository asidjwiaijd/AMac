# Windows 电脑键鼠控制 aMac

桌面软件版本 0.1.0，配合已实测的 aMac 0.6.3 固件。电脑鼠标和键盘通过
USB2 发送给正在运行的 Mac，控制时观察设备屏幕。无需重新烧录固件。

## 使用

1. 保持电池与 USB1 供电/充电连接，用数据线把 USB2 接到 Windows 电脑。
2. 双击 `aMac-Control.exe`。本机也可双击 `scripts/run_amac_control.cmd`。
3. 选择“自动识别 USB2”，点“连接设备”。需要手选时使用实际串口号，
   本机通常是 COM12；关闭正在占用该端口的维护工具或串口监视器。
4. 等设备进入 Mac 桌面，点击“开始控制”。电脑指针会被捕获到控制区域，
   移动、左键、右键、滚轮和键盘输入发送到设备。
5. 按 **Ctrl + Alt + Esc** 释放电脑键鼠。**Alt + Tab** 会先释放控制再切换
   Windows 窗口。窗口失去焦点、移动/缩放或连接中断也会自动释放。
6. 需要更新固件或操作 SD 卡时，先释放，再点“断开连接”，然后打开维护工具。

只有用户点击“开始控制”且控制台位于前台时才转发键鼠；退出捕获会拆除
键盘钩子。程序不保存键入内容。关闭程序会释放已按下的 Mac 按键/鼠标。
断线时无法发送的释放由固件的现有失联保护兜底，重新连接不重放旧输入。

## 按键与鼠标

| 电脑操作 | Mac 收到的操作 |
| --- | --- |
| Windows 键 | Command |
| Alt | Option |
| Ctrl | Control；勾选“将 Ctrl 当作 Mac 的 Command”后改为 Command |
| 左键 | 左键 |
| 右键 | Control + 左键，与本项目实体鼠标约定一致 |
| 中键 | 第三个鼠标按钮，软件是否使用取决于 Mac 应用 |
| 滚轮 | 上/下方向键，滚动效果取决于 Mac 应用 |
| Ctrl + Alt + Esc | 本机退出捕获，不发送该 Esc 到 Mac |
| Alt + Tab | 本机退出捕获并切换 Windows 窗口 |

输入按物理键位传输，使用 Mac 的键盘布局；电脑的中文输入法、剪贴板和
文件不会因此自动同步。Windows 保留的系统安全快捷键仍由系统处理。
捕获前将鼠标速度调到合适位置；控制中退出后可再调整。

当前捕获功能用于 **Mac 正在运行** 的阶段；启动设置里的鼠标操作仍使用
设备上连接的鼠标或原有维护工具。没有实时视频流，设备 LCD 是操作画面。

## 打包与源码运行

独立 EXE 包含 Python、Tk 和串口依赖，使用者无需安装 Python。源码运行
需要带 Tk 的 Windows Python 和 `tools/requirements.txt` 中的 pyserial：

```powershell
python -m pip install -r tools/requirements.txt
python tools/amac_control_app.py
```

开发者安装 PyInstaller 后可运行：

```powershell
.\scripts\build_amac_control.ps1 -Python E:\conda\python.exe
```

本次使用 Python 3.13.9、PyInstaller 6.19.0。输出为
`release/desktop/aMac-Control.exe`，旁置 JSON 记录版本、源码状态和 SHA-256。
构建中间文件位于 `artifacts/desktop-control/`。

通信复用 `MacControl` 的带序号协议，连接时禁止自动复位；单独工作线程
持有串口，连续移动合并发送，按键/点击边沿保持顺序。别名按键独立计数，
避免松开一个 Shift/Ctrl 时误释放另一个；退出捕获优先丢弃旧队列并释放。
过期输入或队列溢出会停止当前捕获，用户重新开始后才继续。

实现依据：[Windows 键盘钩子](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc)、
[光标约束](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-clipcursor)、
[PyInstaller 打包](https://pyinstaller.org/en/stable/usage.html)。
