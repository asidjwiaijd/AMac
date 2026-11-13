# aMac P4C5 移植说明

2026-11-13，版本 `5.4-beta.1+amac.0.6.3`。这是使用用户补充的
`kernel修改说明.zip` 中屏幕初始化表和时序的移植版。已通过内部 USB Serial/JTAG
烧录 P4 v3.2 并校验，实板 SD 读写、配置保存、鼠标识别和 System 7 启动通过。
本版修复复合鼠标状态包被误判为位移的问题，持续接收 USB 中断报告，
输入任务目标间隔改为 1 ms，aMac 显示更新间隔改为 16 ms。
日常固件和 SD 文件可通过 USB2 更新，无需取卡，见[不拆机更新说明](AMAC_UPDATES.md)。
0.5.3 增加独立的电池电压/充电状态诊断，维护工具选择“8 电池与充电状态”
即可查看，不依赖 Mac 的电量百分比服务；详见[电源管理说明](POWER_MANAGEMENT.md)。
0.6.0 改为未充电低于 4.10 V 后直接深睡，主动关机也进入深睡，并定时检测
新的充电信号唤醒；实际限制和维护步骤见[关机与充电唤醒](AMAC_POWER_OFF.md)。
0.6.2 为固定 SDK 的生产版 P4 引导程序增加核心供电稳定等待；首次升级会
校验并备份已知旧引导程序，随后安装时序修复，正常更新继续保留 SD/NVS。
已实测正常关机深睡、重复 RTC 检查及接回 USB1 后充电唤醒进入桌面；
完整 98 项回归通过，真实低电压跨阈值与睡眠电流仍待仪表测试，见[验证记录](AMAC_VALIDATION.md)。
0.6.3 修复 ROM 时钟/PRAM 写入取错寄存器的问题，使日期、时间和时区
设置写入实际数据；联网后自动获取 NTP 时间，使用台北时区，离线继续
RTC 计时，见[自动校时说明](AMAC_TIME.md)。保留 0.6.2 的深睡及充电唤醒实现。
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
| microSD | SDMMC slot 0，保守的1-bit/4 MHz，挂载失败后尝试400 kHz；卡固定3V3供电，IO LDO4固定3.3V，不自动格式化/切换1.8V |
| SD 检测 | GPIO45 只作输入；移除 EV board 把它当电源输出的定义 |
| Wi-Fi | 模组内部 SDIO slot 1；C5 reset54，CLK18/CMD19/D0..3=14..17；已验证扫描、WPA2连接、自动重连与HTTP传输 |
| USB 键鼠 | 经底板U1 hub，以FS/LS兼容模式避开TT限制；按接口/端点解析HID报告，持续接收中断输入；启动设置与Mac复用同一Host |
| 串口 | UART0 TX37/RX38，经USB1串口桥，115200；USB2内部Serial/JTAG同时提供控制和文件维护接口 |
| 电池 | GPIO22 原始电压用于 4.10 V 深睡保护；GPIO21 读取 CHRG 并用于定时检查充电唤醒；向 Mac 报告的百分比估算仍默认关闭 |
| 红外 | GPIO12 仅设为输入；未指定遥控协议/键码，不把 IR_OUT 当 PWM 输出 |
| 音频/触摸 | 这两份网表未给出对应硬件，使用空接口，隐藏 Sound 选项 |
| USB Disk | 禁用：当前 HS USB 路径接 hub，不能用上游 TinyUSB HS device 模式导出 SD 卡 |

GPIO7/8/9/10 是外部扩展接口，不初始化成 Waveshare 的触摸/音频 I2C。
Mac 客户机为 480×360，要求 640×480 等较高分辨率的软件仍可能有超出屏幕的
对话框；本版没有虚拟大桌面缩放。

生日版有5个像素风彩蛋。双击Mac桌面的 **For You** 礼物图标开始，
在控制面板的 **Birthday Settings**、苹果菜单的 **Pocket Terminal** 和
**About This Gift** 中继续探索。桌面 **Start Here** 说明和各页给出后续线索。
这些原生68K小应用由[安装脚本](scripts/install_birthday_desktop.sh)加入磁盘映像；
使用实板时也须将安装后的磁盘映像放到SD卡，单独更新固件不会创建图标。
彩蛋内的键鼠输入由固件接收，
关闭后恢复当前桌面；熄屏时停止动画，低电量关机前收起窗口。
电脑预览已接入同一状态机和绘图代码，见[电脑端说明](examples/amac-desktop/README.md)。
送礼人可查看[彩蛋触发清单和个性化说明](BIRTHDAY_SECRETS.md)（包含剧透）。

实板 System 7 已使用用户提供的 480×360 图片设置桌面壁纸，通过 Décor
随 Mac 启动加载。图片、安装方式及重启验证见[壁纸记录](guest/wallpaper/README.md)。
这项设置保存在实板的 Mac 系统盘中；电脑预览的另一份磁盘需单独安装。

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
- 没有嵌入、下载或自动烧写上游的 C6 固件。实板 C5 原启动区为空，已备份
  完整 Flash 并安装 OSPTEK 的 C5 2.12.9；与 host 2.12.3 的扫描、2.4 GHz
  WPA2 连接、重启自动关联及 HTTP 画面传输通过。本版未验证 5 GHz/BLE。
  来源、校验与维护细节见 [Wi-Fi 记录](AMAC_WIFI.md)。

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

## 上板与 SD 卡

日常更新优先双击 `scripts/update_amac.cmd`，文件维护使用
`scripts/maintain_amac.cmd`。Mac 运行时先在工具中准备维护，再正常关机；工具会拒绝在运行中
更新或修改SD文件。备份、校验、旧文件保留和回退方式见
[不拆机更新说明](AMAC_UPDATES.md)。以下保留首次全量烧录的硬件基线。

2026-11-10 已用 esptool 5.3.1 经内部 USB Serial/JTAG（本机 COM12）完成
Rev3 镜像烧录，写入校验及独立 ROM MD5 均通过。实际芯片为 P4 v3.2，
符合 Rev3 镜像最低要求；原 Flash 前 2 MiB 已备份。固定开发依赖中的旧
esptool 5.2.0 stub 在此芯片上异常，因此本次烧录工具独立安装，没有改动 SDK。
固件控制台仍走 USB1 的 CH343 UART（本机 COM7，切换设为 P4）。完整证据见
[验证记录](AMAC_VALIDATION.md)。

后续使用独立安装的 esptool 5.3.1 可按以下方式读取芯片；端口以实际枚举为准。
只有确认芯片版本及镜像后才执行第二条烧录命令：

```powershell
$env:PYTHONPATH = '.\.pio-core\tools\esptool-5.3.1'
& .\.pio-core\penv\Scripts\python.exe -m esptool --chip esp32p4 --port COM12 --before usb-reset --after no-reset --no-stub chip-id
& .\.pio-core\penv\Scripts\python.exe -m esptool --chip esp32p4 --port COM12 --before usb-reset write-flash 0x0 .\release\aMac-P4C5-HD20034-Rev3.bin
```

SD 卡存放 Mac ROM、虚拟硬盘及启动配置，Mac 保存的文档和系统改动也写入
虚拟硬盘。它是当前启动所需介质，正常使用时应留在设备里。
当前约 4 GB 卡已保留原 FAT32 格式，写入并校验以下三个根目录文件：

| 文件 | 用途 |
| --- | --- |
| `Q650.ROM` | Mac 模拟器启动所需的 ROM |
| `Macintosh.dsk` | 已安装生日图标和彩蛋入口的 System 7 虚拟硬盘 |
| `basilisk_settings.txt` | 16 MB Mac RAM、0 度方向、直接启动，声音/自动 Wi-Fi 关闭 |

文件系统检查通过，设备已成功识别3840 MB卡，找到ROM和系统盘并读入配置。
0.5.0调整为1-bit/4 MHz后，实板配置保存、USB文件上传/下载、原文件备份、
重复启动System 7通过。以前的`ERR BOOT save_failed`已消除。
以后通常无需插拔卡；确需取卡时先正常关闭Mac，再断开设备电源。
用电脑修改磁盘映像时也应先退出电脑模拟器。

准备其他 FAT32 microSD 时，根目录放入自己的 `Q650.ROM` 和 Mac 磁盘镜像。
可复制 [examples/amac-sd/basilisk_settings.txt](examples/amac-sd/basilisk_settings.txt)
到卡根目录，把 `disk` 改成实际文件名。例子关闭声音/Wi-Fi，16 MB 客户机 RAM、
0 度方向，直接启动；配置文件中的路径是 SD 根目录相对路径。

HD20034没有触摸输入。USB鼠标已在启动设置阶段初始化，可直接点击按钮；
USB键盘在Mac中使用。启动设置也可使用USB1/USB2控制工具配置。
需要固定停在设置界面时，可通过文件维护在卡根目录创建
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

向 Mac 报告电量的钩子默认关闭。确认单节锂电池及 ADC 校准后，可显式编译
`AMAC_ENABLE_BATTERY_ESTIMATE=1`。自动休眠保护独立采集原始电压，不依赖该开关。
CHRG 只能证明正在充电，无法区分充满后仍接 USB 与断开 USB。

## 电压保护与休眠

两种 aMac 构建均默认启用 GPIO22 电压保护。8 次 ADC 采样平均后乘 2；
未充电时严格低于 **4.10 V** 持续 5 秒，请求 Finder 正常关机并进入深睡。
正在充电时暂停这项自动关机。已取消原来的 3.9 V 熄屏待机档和 3.7 V 阈值。
保存/取消对话框仍需用户处理，实际 PowerOff 后才卸载 SD 和关闭硬件。

主动关机在清理后保留 10 秒维护窗口，再进入深睡。深睡保留 RTC，每 5 秒
检查充电信号，未检测到新的充电时不初始化 SD、LCD 或 C5，且保持复位脚。
电压反弹不会自动开机。检测到新的充电并再次确认后重新启动 Mac；关机时
已在充电则先等待信号停止，再等待重新开始。运行中的应用和 RAM 不保留。

现有 GPIO21 不是深睡唤醒引脚，也没有独立 VBUS 检测，因此是周期性检查
开始充电，满电插线不能保证唤醒。准备维护、实际限制和协议诊断见
[关机与充电唤醒说明](AMAC_POWER_OFF.md)。

RTC使用固定SDK的内部RC时钟，深睡中可能随温度漂移。P4早期工程样片还受
DIG-399的数字GPIO保持限制影响，需测量C5复位在深睡中的实际状态。SD、USB Hub、
稳压器和电池100K+100K分压支路仍由电路供电，整机休眠电流需要实测。
RTC连续计时依赖板卡持续供电。依据见
[乐鑫睡眠模式](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/system/sleep_modes.html)、
[系统时间](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/system/system_time.html)及
[Apple关机事件说明](https://developer.apple.com/library/archive/documentation/mac/pdf/Processes/Shutdown.pdf)。

## 电脑端预览

在 Windows 双击 [`scripts/run_amac_desktop.cmd`](scripts/run_amac_desktop.cmd)，
用 WSL Ubuntu 的桌面版 Basilisk II 启动 System 7。客户机配置为 68040、16 MiB
内存和 480×360。准备方法和上游镜像的寻盘问题见
[电脑端预览说明](examples/amac-desktop/README.md)。HD20034 的点亮和颜色仍需上板验证。

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

本版完整90项回归在WSL Ubuntu运行：

```powershell
wsl -d Ubuntu -- env CXX=clang++ AMAC_SANITIZERS=address,undefined python3 -m unittest discover -s test -v
```

需在仓库根目录运行；最终构建来源、哈希与桌面操作结果见
[验证记录](AMAC_VALIDATION.md)。

后续同步先 `git fetch upstream`，在移植分支合并/挑选需要的上游提交，再重复
以上检查及两种芯片配置构建。不要用 `master` 覆盖移植分支。构建时的提交及
dirty 状态保存在镜像旁 JSON，实机验证结果可继续追加到 [AMAC_VALIDATION.md](AMAC_VALIDATION.md)。
