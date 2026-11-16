# aMac

一台能跑经典 Mac 的掌上小电脑：ESP32-P4/C5 主板，配一块 480×360 的屏幕，开机进入 System 7。
这个仓库是它的固件，从 [M5Tab-Macintosh](https://github.com/amcchord/M5Tab-Macintosh)
的 BasiliskII 移植而来。

当前固件 **0.6.4**（`5.4-beta.1+amac.0.6.4`），Windows 键鼠控制台 **0.1.0**。

![aMac 生日版桌面](guide/images/desktop.png)

## 先读哪份

| 你想做什么 | 从这里开始 |
| --- | --- |
| 第一次开机、日常使用 | [新手图文指南](AMAC_USER_GUIDE.md)，或 [PDF 版](output/pdf/aMac_User_Guide.pdf) |
| 用电脑的键鼠操作设备 | [Windows 键鼠控制台](AMAC_DESKTOP_CONTROL.md)，软件在[下载页](https://github.com/asidjwiaijd/AMac/releases/tag/amac-control-v0.1.0) |
| 更新固件、备份或替换 SD 卡文件 | [不拆机更新](AMAC_UPDATES.md) |
| 构建、烧录、改代码 | [移植说明](AMAC_PORT.md) |
| 准备礼物、查彩蛋答案 | [彩蛋清单](BIRTHDAY_SECRETS.md)（有剧透） |

## 硬件

- **主板**：OSPTEK ESP32-P4C5，引脚按 2026-11-02 的底板和前板网表接线，
  依据见 [hardware/README.md](hardware/README.md)。实机芯片为 P4 v3.2。
- **屏幕**：HD20034C40-V1（规格书内型号 HD20034C40-H-V1），ST7701S，两线 MIPI DSI，480×360。
  初始化表和时序取自 `kernel修改说明.zip`，见[屏幕驱动参考](hardware/reference/HD20034_KERNEL_REFERENCE.md)。
- **无线**：板上的 ESP32-C5 经 SDIO 连接 P4，运行 OSPTEK 的 C5 2.12.9 固件，详见 [Wi-Fi 记录](AMAC_WIFI.md)。
- **接口**：USB1 负责供电、充电和调试串口；USB2 是 P4 的 USB Serial/JTAG，用来连电脑做控制和维护；
  USB 键鼠经底板 hub 接入；microSD 存放 ROM、Mac 系统盘和配置。
- **没有的**：触摸、音频。背光只能开或关。

## 能做什么

- 运行 System 7，Mac 画面 480×360、1:1 显示，支持 USB 鼠标和键盘。
- 生日版桌面：**For You** 礼物入口、5 个像素风彩蛋，以及用 Décor 加载的壁纸。
- 连上 Wi-Fi 后自动校时（台北时间，UTC+8），断网时靠 RTC 继续走时。
- 电池保护：未充电且电压低于 4.10 V 持续 5 秒，Mac 会正常关机并进入深睡；
  深睡中检测到开始充电就自动开机。手动 Shut Down 也会进入深睡。
- 不拆机维护：通过 USB2 更新固件、查看电池和校时状态、备份和替换 SD 卡上的文件。
- 电脑键鼠控制：Windows 程序把电脑的键盘和鼠标经 USB2 转发给设备。

## 构建

在仓库根目录用 PowerShell 运行：

```powershell
.\scripts\build_amac.ps1
```

- 默认环境 `amac_p4_rev3`，面向生产版芯片，要求芯片 revision 至少 3.1。
  旧的工程样片用 `.\scripts\build_amac.ps1 -Environment amac_p4_es`。
- 产物在 `release/`：`aMac-P4C5-HD20034-Rev3.bin` 是完整合并镜像（烧录偏移 0x0），
  旁边的 JSON 记录环境、源码提交和 SHA-256。
- SDK、工具链和缓存默认放在项目内的 `.pio-core`。第一次构建需要联网下载固定版本的工具链。
- 固定的依赖版本、首次全量烧录和 SD 卡准备方法见[移植说明](AMAC_PORT.md)。

运行回归测试（WSL Ubuntu）：

```powershell
wsl -d Ubuntu -- env CXX=clang++ AMAC_SANITIZERS=address,undefined python3 -m unittest discover -s test -v
```

不用实机也能在电脑上预览系统和生日应用：双击 `scripts/run_amac_desktop.cmd`，
说明见 [examples/amac-desktop](examples/amac-desktop/README.md)。

## 日常维护

装好以后，平时不需要再拆机或拔 SD 卡：

- `scripts/update_amac.cmd`：更新固件，自动检查、备份、写入和校验。
- `scripts/maintain_amac.cmd`：查看和传输 SD 卡文件、查看电池与校时状态、进入启动设置。

两个工具都通过 USB2 工作。改动 SD 卡或写固件前，Mac 必须先正常关机，工具会提示怎么做。
详细步骤见[不拆机更新](AMAC_UPDATES.md)和[关机与充电唤醒](AMAC_POWER_OFF.md)。

## 版本记录

| 版本 | 主要变化 |
| --- | --- |
| 0.6.4 | 修复运行中低电压不触发关机的问题；实机验证了自动深睡和充电唤醒 |
| 0.6.3 | 修正 ROM 时钟/PRAM 写入；联网自动校时，使用台北时区 |
| 0.6.2 | 修复 P4 v3.2 从深睡唤醒时的引导时序，充电唤醒可以正常进入桌面 |
| 0.6.0 | 低于 4.10 V 直接深睡，主动关机也进入深睡，定时检测充电唤醒 |
| 0.5.3 | 新增电池电压和充电状态诊断 |
| 0.5.0 | 通过 USB2 维护 SD 卡文件，SD 改用更保守的 1-bit/4 MHz |

更早的版本见 Git 标签 `amac-v*`，每个版本的实机测试结果见[验证记录](AMAC_VALIDATION.md)。

## 已知限制

- 没有触摸和音频；背光只有开和关，软件里的亮度百分比没有调光效果。
- Wi-Fi 只验证了 2.4 GHz WPA2；5 GHz 和蓝牙没有验证。
- 整机深睡电流、完整续航和 RTC 长期误差还没有测量。
- Mac 画面只有 480×360，要求 640×480 的软件可能有对话框超出屏幕。
- 不支持把 SD 卡作为 U 盘挂到电脑（USB Disk 模式已禁用）。
- 上游的 Tab5 / Waveshare 固件不能直接刷到 aMac 上。

## 全部文档

| 文档 | 内容 |
| --- | --- |
| [AMAC_USER_GUIDE.md](AMAC_USER_GUIDE.md) | 新手使用指南 |
| [AMAC_DESKTOP_CONTROL.md](AMAC_DESKTOP_CONTROL.md) | Windows 键鼠控制台 |
| [AMAC_UPDATES.md](AMAC_UPDATES.md) | 不拆机更新固件和维护 SD 卡 |
| [AMAC_POWER_OFF.md](AMAC_POWER_OFF.md) | 关机、深睡和充电唤醒 |
| [AMAC_TIME.md](AMAC_TIME.md) | 自动校时 |
| [AMAC_WIFI.md](AMAC_WIFI.md) | C5 无线固件和 Wi-Fi |
| [AMAC_PORT.md](AMAC_PORT.md) | 移植细节、构建、烧录和测试 |
| [AMAC_VALIDATION.md](AMAC_VALIDATION.md) | 每个版本的实机验证记录 |
| [hardware/README.md](hardware/README.md) | 网表和引脚依据 |
| [BIRTHDAY_SECRETS.md](BIRTHDAY_SECRETS.md) | 生日彩蛋和个性化（有剧透） |
| [guest/wallpaper/README.md](guest/wallpaper/README.md) | 壁纸来源和安装 |
| [GIT_IDENTITY.md](GIT_IDENTITY.md) | 提交署名记录 |
| [UPSTREAM_README.md](UPSTREAM_README.md) | 上游 M5Tab-Macintosh 的原始 README |

## 上游、维护与许可

- 基于 [amcchord/M5Tab-Macintosh](https://github.com/amcchord/M5Tab-Macintosh) 提交 `3641580d72982cec464d4800f2ead507ba7c687d`，
  保留完整 Git 历史，基线标签 `amac/upstream-2026-11-03`。上游的原始说明移到了 [UPSTREAM_README.md](UPSTREAM_README.md)。
- 移植分支 `amac/esp32p4-hd20034`，仓库 [asidjwiaijd/AMac](https://github.com/asidjwiaijd/AMac)。
- 移植维护：[LitterCarrot (@asidjwiaijd)](https://github.com/asidjwiaijd)。历史提交的署名修正见[提交署名记录](GIT_IDENTITY.md)。
- 许可沿用上游：项目基于 BasiliskII，使用 GNU General Public License v2。
  壁纸素材的权利归原作者，见[壁纸记录](guest/wallpaper/README.md)。
