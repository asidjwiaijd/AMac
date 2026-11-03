# aMac P4C5 移植说明

2026-11-03，版本 `5.4-beta.1+amac.0.1.1`。这是使用用户补充的
`kernel修改说明.zip` 中屏幕初始化表和时序的移植版，尚未连接实板验证。
完整依据和格式适配见 [屏幕驱动参考](hardware/reference/HD20034_KERNEL_REFERENCE.md)。

## 代码与硬件基线

- 上游：[amcchord/M5Tab-Macintosh](https://github.com/amcchord/M5Tab-Macintosh)，
  提交 `3641580d72982cec464d4800f2ead507ba7c687d`，保留完整 Git 历史。
- 工作分支：`amac/esp32p4-hd20034`。原始基线标签：`amac/upstream-2026-11-03`。
- 上游 remote 命名为 `upstream`；本次只有本地提交，没有发布到上游。
- 输入：`Netlist_sch_bottom_2026-11-02.tel`、
  `Netlist_sch_front_2026-11-02-1.tel`，快照与逐段连线见
  [hardware/README.md](hardware/README.md)。不使用较旧网表。
- 屏幕：`HD20034C40-V1规格书.pdf`，内部型号 HD20034C40-H-V1，
  ST7701S，原生 480×360；软件和 Mac 客户机均按此尺寸显示。

## 已接入的功能

新增 `amac-p4c5` 专属板型与 Arduino 引脚变体。`src/board/amac/` 实现显示、
存储、C5 通信、板级生命周期及无音频/无触摸的接口。原 Tab5/Waveshare
配置仍可单独构建。

| 部分 | 实现与边界 |
|---|---|
| LCD | 两条 MIPI DSI 数据 lane，240 Mbps/lane；附件41条初始化命令；RGB565、单个345600字节 scanout framebuffer；GPIO23复位 |
| 显示管线 | 新增原生横屏映射，1:1 显示；0/180 度旋转、1/2/4/8 bit 索引色、截图/像素校验共用同一坐标 |
| 启动菜单 | 480 像素宽时把 Memory/Options 改为上下排列，媒体列表保留滚动 |
| 背光 | GPIO13 开关，默认高电平开启；亮度/LED 电流由前板 R3 模拟电路调整，软件百分比只有开/关效果 |
| microSD | SDMMC slot 0，4-bit，20 MHz，失败后尝试 400 kHz；卡固定3V3供电，IO LDO4 固定3.3V，不自动格式化/切换1.8V |
| SD 检测 | GPIO45 只作输入；移除 EV board 把它当电源输出的定义 |
| Wi-Fi | 模组内部 SDIO slot 1；C5 reset54，CLK18/CMD19/D0..3=14..17；同版本源码重编通信入口，校验 C5 |
| USB 键鼠 | 复用上游 USB Host/HID/hub 路径，经底板 U1 hub；SDK 已启用多层 hub |
| 串口 | UART0 TX37/RX38，经调试切换和 USB1 串口桥，115200；USB2 不作为默认控制台 |
| 电池 | GPIO22 电压分压、GPIO21 CHRG 钩子已保留，默认不向 Mac 报告电量；缺少电池参数与独立 VBUS/电流检测 |
| 红外 | GPIO12 仅设为输入；未指定遥控协议/键码，不把 IR_OUT 当 PWM 输出 |
| 音频/触摸 | 这两份网表未给出对应硬件，使用空接口，隐藏 Sound 选项 |
| USB Disk | 禁用：当前 HS USB 路径接 hub，不能用上游 TinyUSB HS device 模式导出 SD 卡 |

GPIO7/8/9/10 是外部扩展接口，不初始化成 Waveshare 的触摸/音频 I2C。
Mac 客户机为 480×360，要求 640×480 等较高分辨率的软件仍可能有超出屏幕的
对话框；本版没有虚拟大桌面缩放。

## 固定的开发依赖

- PlatformIO + pioarduino 提交 `fbdfc2962e468ca3c4b51811ae691ed887df903c`
  （55.03.38-1 系列），Arduino 3.3.8 / ESP-IDF 5.5.4。
- ST7701 MIPI 驱动从乐鑫 esp-iot-solution 提交
  `8f2257f7e88aeba4c325fe288a5121b92900391c` 随源码保存，见
  [lib/esp_lcd_st7701/UPSTREAM.md](lib/esp_lcd_st7701/UPSTREAM.md)。
- SDK 自带的 ESP-Hosted 2.12.3 按 C6 编译，会在发现 C5 时断言。
  本版从乐鑫提交 `be55ec7141c0efa4796191f273512796542b573d` 保存相同版本
  transport 源码，按 C5 重新编译这一单元。其余组件继续使用固定 SDK。
  版本守卫和链接 map 检查会拒绝版本漂移或误链接 C6 transport。
  详见 [third_party/esp_hosted_2_12_3/UPSTREAM.md](third_party/esp_hosted_2_12_3/UPSTREAM.md)。
- 没有嵌入、下载或自动烧写上游的 C6 固件。OSPTEK 现有 C5 固件为 2.12.9，
  比 host 新，仍需做实际联网兼容测试。本版未验证 5 GHz/BLE 扩展能力。

## 构建和产物

在 `E:\AMac\firmware` 用 PowerShell 执行：

```powershell
# 本机已安装 PlatformIO；新机器可使用 uv tool install platformio。
.\scripts\build_amac.ps1
```

脚本默认把 SDK、工具、缓存及临时文件放入项目内 `.pio-core`（本次在 E 盘），
避免大体积 SDK 填满系统盘。也可自行设置 `PLATFORMIO_CORE_DIR`。
构建需联网获取固定版本的工具链；主机检查需要 Python 3 和 C++17 编译器
（本机为 G++ 15.2）。

默认 `amac_p4_rev3` 使用生产芯片 SDK，**最低芯片 revision 3.1**。
模组名/P4C5/PCB 版本不能代替芯片 revision；v3.0 不在该 SDK 支持范围。
显式旧工程样片配置：

```powershell
.\scripts\build_amac.ps1 -Environment amac_p4_es
```

| 文件 | 用途 |
|---|---|
| `release/aMac-P4C5-HD20034-Rev3.bin` | 生产芯片完整合并镜像，烧录偏移 **0x0** |
| `release/aMac-P4C5-HD20034-ES.bin` | 旧工程样片镜像，烧录偏移 **0x0** |
| `release/*.json` | 对应环境、最低 revision、源码提交/是否含未提交改动及 SHA256 |
| `.pio/build/<env>/firmware.bin` | 仅应用镜像，偏移 **0x10000** |
| `.pio/build/<env>/firmware.map` | 链接证据，检查实际采用 C5 transport |

构建结束会逐字节比较镜像内的 bootloader（0x2000）、partition（0x8000）和
application（0x10000），并检查芯片 ID 与 SDK revision。生成文件、SDK 与
Mac ROM/磁盘镜像不进 Git。

## 初次上板

本次没有执行实物烧录。以下 COM7 是示例，应替换为实际调试口；使用底板 USB1
串口桥，并把烧录切换设为 P4。先读取芯片版本，再选对应镜像：

```powershell
& .\.pio-core\penv\Scripts\python.exe .\.pio-core\packages\tool-esptoolpy\esptool.py --port COM7 chip-id
& .\.pio-core\penv\Scripts\python.exe .\.pio-core\packages\tool-esptoolpy\esptool.py --chip esp32p4 --port COM7 write-flash 0x0 .\release\aMac-P4C5-HD20034-Rev3.bin
```

准备 FAT32 microSD，根目录放入自己的 `Q650.ROM` 和 Mac 磁盘镜像。
可复制 [examples/amac-sd/basilisk_settings.txt](examples/amac-sd/basilisk_settings.txt)
到卡根目录，把 `disk` 改成实际文件名。例子关闭声音/Wi-Fi，16 MB 客户机 RAM、
0 度方向，直接启动；配置文件中的路径是 SD 根目录相对路径。

HD20034 没有触摸输入。现有 USB 键鼠驱动在进入模拟器后初始化，启动设置阶段
可以编辑 SD 配置文件或使用串口控制工具。需要停在设置界面时，在卡根目录创建
空文件 `basilisk_recovery.txt`，配置完成后删除它。

```powershell
python tools/mac_control.py --port COM7 info
python tools/mac_control.py --port COM7 boot-get
python tools/mac_control.py --port COM7 boot-list disk
python tools/mac_control.py --port COM7 boot-set disk /Macintosh.dsk
python tools/mac_control.py --port COM7 boot-save
python tools/mac_control.py --port COM7 boot-start
python tools/mac_control.py --port COM7 screenshot amac-screen.png
python tools/mac_control.py --port COM7 panel-verify
```

控制工具需要 `pyserial`（`python -m pip install -r tools/requirements.txt`）。
同一串口同一时间只由一个工具占用；截图和坐标均为480×360。
Wi-Fi 可通过 SD 文件设置 `wifi_ssid`、`wifi_pass`、`wifi_auto=yes`，或用
`boot-set` 配置后 `boot-wifi-connect`。本版不替换 C5 从机固件。

## 屏幕驱动参数与实机验证

屏幕初始化使用补充附件 `tspi-rk3566-dsi-v10.dtsi` 的41条 DCS 命令，包含完整
gamma/GIP 数据及每条命令后的延时。通过 `st7701_vendor_config_t.init_cmds`
传给驱动，替换首版的乐鑫默认表。15 MHz pixel clock；水平 FP/BP/SYNC=
48/43/10，垂直=18/28/10，2 lanes × 240 Mbps，按总时序计算约62.06 Hz。
复位低电平保持200 ms，释放后等120 ms；按附件保留准备20 ms和使能前150 ms。

附件 Linux 配置为 RGB888；当前 MiniGfx/模拟器帧缓冲为 RGB565，因此 P4 的
framebuffer、DSI 数据格式及 ST7701 COLMOD=0x55 都保持 RGB565。没有单独把
DSI 改成 RGB888 后继续发送两字节像素。该格式适配同样需要实机颜色验证。

参考文件保存在 `hardware/reference/tspi-rk3566-dsi-v10.dtsi`；转换工具为
`scripts/generate_hd20034_profile.py`，生成并跟踪
`src/board/amac/hd20034_panel_profile.h`。构建会用 `--check` 拒绝过期配置。
新面板数据确认后修改参考来源、重新生成并运行测试；背光极性仍在
`src/board/amac/amac_pins.h`，P4 GPIO 始终以最新网表为准。

硬件验证应覆盖：排线和插座的实际编号方向、背光极性/模拟电流、LCD ID 与
纯色图/旋转/持续刷新、SD 长时间读写、hub 后键鼠热插拔、C5 与 SD 并发运行。
这份网表只能证明连接，无法证明插座接触面方向、器件型号及实际出厂芯片版本。

电池钩子默认关闭。确认单节锂电池及 ADC 校准后，可显式编译
`AMAC_ENABLE_BATTERY_ESTIMATE=1`；仅是电压估计，不提供剩余时间。CHRG 只能
证明正在充电，无法区分充满后仍接 USB 与断开 USB，不把这项估计当作电量计。

## 验证命令与版本管理

```powershell
python -m unittest discover -s test -p test_amac.py -v
$env:PYTHONPATH = '.\test'
python -m unittest test_issue_fixes.ReleaseImagesTests test_issue_fixes.DSIPatchTests -v
git status --short --branch
```

aMac 检查包含附件41条DCS命令的逐字节比对、复位/背光/失败清理、网表/引脚
对应、厂商源码校验、三种板型的随机显示管线、所有索引
色深、两种旋转、失败重试、生命周期及启动界面像素一致性。界面截图在
`artifacts/amac-ui/`。本机原生检查使用 G++；需要 ASan 时在支持的环境设置
`CXX=clang++` 和 `AMAC_SANITIZERS=address,undefined`。

后续同步先 `git fetch upstream`，在移植分支合并/挑选需要的上游提交，再重复
以上检查及两种芯片配置构建。不要用 `master` 覆盖移植分支。构建时的提交及
dirty 状态保存在镜像旁 JSON，实机验证结果可继续追加到 [AMAC_VALIDATION.md](AMAC_VALIDATION.md)。
