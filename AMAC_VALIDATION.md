# aMac 验证记录

## 0.5.3：独立充电检测实测（当前版本）

2026-11-13，P4 固件 `5.4-beta.1+amac.0.5.3`。两种镜像均由干净源码提交
`0e49714ab3b1d9a177b2acf8b35bb90b9aeab7f1` 构建，`source_dirty=false`。
版本标签 `amac-v0.5.3-charging-detection`；后续文档提交不改变固件。

| 环境 | 构建 | 合并镜像字节数 | 最低 revision |
| --- | --- | ---: | --- |
| `amac_p4_rev3` | SUCCESS，94.468 秒 | 1956976 | 3.1 |
| `amac_p4_es` | SUCCESS，81.523 秒 | 1954144 | 0.1 |

```text
aMac-P4C5-HD20034-Rev3.bin
5d8fb387740ad17e09e1ed92153c6b76f5849b15f4a9c28c5df7a987b9f0a414

aMac-P4C5-HD20034-ES.bin
e6883cf8e4a9db2ca2dd24592aa96ea22c1d9e5a3f7ea750299b19d9a673fff6
```

两份合并镜像的偏移、P4 镜像头、组件逐字节内容、SHA-256、来源、版本
字符串及 C5 transport 链接均核对通过。完整 92 项回归通过，140.617 秒；
支持的 C++ 检查启用 ASan/UBSan。新增检查使用实际板级驱动，覆盖充电
防抖、短暂跳变、无效 ADC、缓存读取不触碰 ADC、超过 5 秒的旧采样、
采样中断后重新确认及毫秒回绕；维护工具检查旧固件兼容、无效/过期状态、
充电判断独立于关闭的客机电池服务，且不把高电平判作“充满”。

实板 BOM `BOM_bottom_v2.2_pcb_bottom_2026-11-03.xlsx` 的 U9 为富满
`LTH7R.`，LCSC C841234。CHRG pin 1 经 10 kΩ 上拉接 GPIO21。
Mac 在 Finder 正常关机后，经 USB2 COM12 仅更新 P4 应用 `0x10000`；
原 0.5.2 应用先与目标 Flash 的 ROM 摘要匹配并备份。写入校验及独立 ROM
复验均通过，新应用 SHA-256：
`43098abf741814b7c8585e58f2d0a89760dc792ba1bdeede5492d2e6eff9c88e`。
记录为 `artifacts/updates/2026-11-13_021800_287614/result.json`。

新固件启动后进入 Finder；再次正常关机进入维护设置，用户按要求拔掉
USB1，保留电池与 USB2，然后接回 USB1。实际读取如下，全部
`simulated=0`、`raw_valid=1`，采样年龄均小于 1 秒：

| 阶段 | 采样数 | 实测电压范围 | GPIO21 | `raw_charging` |
| --- | ---: | --- | --- | --- |
| 更新后，USB1 一直连接 | 12 | 4216–4220 mV | 高 | 首次 -1，稳定后 0 |
| 维护页，纯电池 | 15 | 4168–4176 mV | 高 | 全部 0，未在充电 |
| 维护页，接回 USB1 | 20 | 4224–4228 mV | 低 | 全部 1，正在充电 |

纯电池和接回 USB1 的会话均为 `0CBD2E61`，切换未重启 P4；维护页
持续采样，采样计数递增。初始 USB1 接入但 CHRG 高，证明“插线”与
“正在充电”需要区分；本次没有据此确认充满。接回 USB1 后 CHRG 明确
变低，验证了实际充电信号，而不只是电压回升。维护菜单选项 8 在维护页
和 Mac 启动后均实测可用，显示“正在充电”。

最后重新启动 Mac 并检查 480×360 彩色桌面：壁纸、Start Here、For You
图标均可见。运行阶段 `raw_mv=4230`、`raw_valid=1`、`raw_charging=1`，
采样计数继续增加，采样年龄 591 ms。USB1 已接回，设备留在正常桌面。
证据为 `final-status.json`、`final-desktop.png`。

电压绝对精度仍缺万用表校准；本次未测充电电流、完整充满周期、RTC
唤醒和深睡电流。充电芯片独立管理充电，此功能只读取状态。
没有独立 VBUS 检测，CHRG 高时不能区分未插线与已停止充电。
Mac 百分比服务保持默认关闭，3.9/3.7 V 电压策略保持原值。

证据目录 `artifacts/charging/2026-11-13/`：完整软件/双构建日志、发布
校验、更新日志、三阶段采样 JSON 与 `sampling-summary.json`、维护菜单
日志、更新前桌面/关机菜单和维护页截图。历史版本和回退文件均保留。

## 2026-11-13：电池电压采样与供电切换实测

当前源码的三项定向检查全部通过，5.900 秒：aMac 电压阈值/ADC 换算、
跨深睡的 Mac 时钟状态、通用电池估算与背光。支持的检查使用 ASan/UBSan。
日志：`artifacts/battery/2026-11-13/software-checks.log`。

网表对应为 GPIO22 / ADC1 channel 6，R34/R35 各 100 kΩ、C23 为 100 nF，
测量分压乘 2；GPIO21 读取带 10 kΩ 上拉的充电状态脚。软件检查覆盖
严格低于 3900 mV 的待机、低于 3700 mV 的深睡、连续 5 秒确认、输入后
60 秒待机宽限、无效采样和计时回绕。时钟测试模拟重复 300 秒深睡；实板
RTC 唤醒间隔参数为 `300000000` 微秒。

用户确认连接电池和 USB2 数据线，无万用表。用现有固件的 `POWER.raw_mv`
读取实板电压，共 100 次采样，全部 `simulated=0`，没有修改或烧录固件。

| 阶段 | 采样数 | 读数范围 | 平均读数 |
| --- | ---: | --- | --- |
| USB1 接入，游戏运行 | 15 | 4208–4212 mV | 4208.267 mV |
| 纯电池启动 Mac 后 | 35 | 4150–4168 mV | 4159.143 mV |
| 纯电池稳定运行，另一段采样 | 25 | 4142–4146 mV | 4143.760 mV |
| 重新接回 USB1 | 25 | 4214–4218 mV | 4216.560 mV |

切换前由用户保存并退出 SimCity，正常关闭 Mac 后拔掉 USB1，仅保留电池
与 USB2。此时仅枚举 COM12，P4 仍能响应；随后在电池供电下重新启动 Mac
并进入 Finder。按最新网表，USB2 的 VBUS 脚 2/11 未连接，USB1 是此次
外部供电/充电输入。纯电池读数下降，恢复 USB1 后回升，证明采样随实际
供电状态更新。恢复 USB1 前后的启动会话均为 `1A207DCB`，未发生 P4 重启。
纯电池及恢复供电的采样期间显示保持 awake，休眠次数保持 0；电压均高于
3.9 V，与当前策略预期一致。测试结束时 USB1 已恢复连接，Mac 在运行。

实测通过的范围是电池供电、启动、实时电压采集和供电切换。无万用表，
ADC 的绝对精度尚未校准；本次电压较高，没有实机触发 3.9 V 待机、3.7 V
深睡、RTC 定时恢复或测量深睡电流。当时的协议只公开用于保护的 `raw_mv`，
未单独验收 GPIO21 充电指示电平。

Mac 电量百分比服务仍默认关闭，所以 `battery/present/percent/mv/charging`
等客机服务字段为 0；这不等于此次读取的原始电池电压为 0。低电压保护独立
使用 `raw_mv`，`POWER SIM` 的客机数值模拟不能替代原始采样验证。

实板证据位于 `artifacts/battery/2026-11-13/`：

- `usb-baseline.json`、`battery-only-start.json`
- `battery-only-samples.json`、`battery-only-steady.json`、`usb-restored.json`
- `sampling-summary.json`、`battery-only-desktop.png`、`final-battery-desktop.png`

## 0.5.2：C5 Wi-Fi 恢复与错误处理

2026-11-11，P4 固件 `5.4-beta.1+amac.0.5.2`。两种镜像均由干净源码提交
`42b6dce0818016a64d839fdc228b0f54db41fcd3` 构建，JSON 中
`source_dirty=false`。后续文档提交不改变固件；版本标签为
`amac-v0.5.2-wifi-recovery`。

| 环境 | 构建 | 最低芯片 revision | 合并镜像字节数 |
| --- | --- | --- | ---: |
| `amac_p4_rev3` | SUCCESS，22.800 秒 | 3.1 | 1956432 |
| `amac_p4_es` | SUCCESS，22.162 秒 | 0.1 | 1953664 |

```text
aMac-P4C5-HD20034-Rev3.bin
788d58ddb783412adaf7abb87a31166986a91db3bc8a06d0f2266f841a3e2992

aMac-P4C5-HD20034-ES.bin
c53051f181d23d7b13044b6f0b79e77e3087e96941397bf65fd53fc11a8f7e0d
```

合并偏移、P4 镜像头、组件及整包 SHA-256、应用版本和来源均已核对。
完整 90 项回归通过，118.339 秒，支持的 C++ 检查开启 ASan/UBSan。
新增失败无线驱动测试直接提取生产初始化/扫描代码，验证初始化失败后
停止扫描、重试可恢复、成功后不重复初始化、取消关联及扫描失败可重试。
设置页同时区分模块启动失败和扫描失败，并为扫描/关联设置等待上限。

根因在 C5 启动区：P4 的 SDIO 初始化超时，C5 UART 反复报告无效的
`0xffffffff` 启动头。完整备份证实 C5 前 64 KiB 为空。已为 C5 v1.2 安装
厂商 ESP-Hosted 2.12.9 合并包并经独立 ROM 验证，保存了经目标摘要校验的
4 MiB 全片备份。来源、哈希和具体布局见 [Wi-Fi 修复记录](AMAC_WIFI.md)。

安装 C5 后，原 P4 0.5.1 即恢复扫描；用户完成连接后，电脑通过设备 Wi-Fi
HTTP 通道收到 200 响应及 53965 字节画面。随后从 Finder 正常关机，通过
USB2 COM12 只更新 P4 应用 `0x10000`；保存匹配当前 Flash 的 0.5.1 回退
副本，写入与独立 ROM 校验通过。新应用 SHA-256：
`a4caf947827aad890dca00798536d9400500626691f5aef622a7c50208714c30`。
更新记录：`artifacts/updates/2026-11-11_142806_713158/result.json`。

0.5.2 启动后，C5 日志确认自动关联 2.4 GHz channel 1 / WPA2，P4 获得
网络地址并进入 Mac。电脑经 Wi-Fi 下载 53992 字节、CRC 检查并解码为
480×360 画面；壁纸、For You 和 Start Here 均保留。未重传或替换系统盘。
重置输入诊断后连续进行四次 Wi-Fi 画面下载，USB 传输错误为 0，最大
服务间隔 2241 µs；未作为鼠标输入的厂商状态包继续正常忽略。
本轮未测试 5 GHz、BLE 或 Mac 客户机浏览器。

证据：

- `artifacts/build/amac-0.5.2-complete-tests-final.log`
- `artifacts/build/amac-p4-*-wifi-recovery.log`
- `artifacts/wifi/2026-11-11/c5-startup.log`、`scan-completed.png`
- `c5-repair-141434/result.json`、`previous-c5-flash.bin`
- `p4-release-verification.json`、`p4-update.log`
- `c5-final-restart.log`、`final-connection.json`、`final-wifi-desktop.png`
- `wifi-and-usb-runtime.json`

## 2026-11-11：System 7 用户壁纸

在实板 Mac 内安装 Décor 3.0.4（Custom → Universal Install），将用户提供的
`E:/AMac/wallpaper.png` 转为同尺寸 256 色 GIF，复制到
`MicroMac7:Wallpaper.gif` 并设为桌面背景。源图及 GIF 已保存至
`guest/wallpaper/`，文件哈希、第三方工具来源和安装说明见该目录 README。

安装盘 `Wallpaper.iso` 819200 字节，经 USB2 上传及完整 CRC32 校验通过。
完成后正常关机、清除临时 CD-ROM 配置，再启动 Mac，壁纸自动恢复，
For You 与 Start Here 图标保留。所有可读取启动选项与安装前副本逐项一致。
本次只改变实板 Mac 系统盘内的程序、图片与偏好设置，固件仍为下方 0.5.1；
没有替换本地电脑预览所用的另一份系统盘。

首次 Easy Install 的主程序资源表不一致，报告 `-199`。覆盖做 Universal
Install 仅更新 Daemon 后仍未恢复；将新装的主程序移到废纸篓，重新完整
Universal Install 后正常。保留废纸篓中的副本，未清空用户废纸篓。

证据位于 `artifacts/wallpaper/2026-11-11/`：

- `prepared.json`、`conversion-check.json`、`media-transfer.json`
- `original-settings.json`、`restored-settings.json`
- `decor-header.bin`、`decor-map.bin`、`daemon-header.bin`、`daemon-map.bin`
- `clean-universal-result.png`、`decor-settings.png`
- `wallpaper-desktop-color.png`、`wallpaper-after-restart.png`

## 0.5.1：鼠标延迟与乱跳修复（历史记录）

2026-11-11，固件 `5.4-beta.1+amac.0.5.1`。两种芯片镜像均来自干净源码
提交 `8ec24003be26cba5973cb66fc968f4c00435733b`，旁置 JSON 均为
`source_dirty=false`。后续测试夹具和验证文档提交不改变固件二进制。
版本标签为 `amac-v0.5.1-mouse-response`。

| 环境 | 最终构建 | 最低芯片 revision | 合并镜像字节数 |
| --- | --- | --- | ---: |
| `amac_p4_rev3` | SUCCESS，47.708 秒 | 3.1 | 1955504 |
| `amac_p4_es` | SUCCESS，53.155 秒 | 0.1 | 1952736 |

```text
aMac-P4C5-HD20034-Rev3.bin
bf21b76078c0b42d916a19defa70b2604c13b19dcb6322f278030e574565a49e

aMac-P4C5-HD20034-ES.bin
18dc12ae515e7a1ee2ffef053f73be665d042475b766b0895ff03e8cc336314b
```

两份合并镜像的组件偏移、P4 镜像头、组件及整包 SHA-256、应用版本与源码
来源均已再次核对，结果保存于下述 `release-verification.json`。

输入任务由 20 ms 改为 1 ms 目标间隔，USB Host 事件处理不再附加阻塞
等待，interrupt-IN 传输在完成回调中连续重新提交。板级 ADC 等轮询保留
20 ms。每个 HID 接口的描述符只绑定该接口的端点，未知/待解析报告不再
猜测为鼠标。解析器只接受相对 X/Y，补齐 Global PUSH/POP 与 usage range。
aMac 的显示信号及最小刷新间隔均为 16 ms，其他板型的间隔保持原值。

完整 89 项回归全部通过，115.941 秒，支持的 C++ 检查使用 ASan/UBSan。
新增测试直接包含生产 USB Host 实现，以异步模拟 SDK 检查连续接收、接口
隔离、禁止重复提交、延时错误恢复、断连后排空、设备槽复用以及键盘 LED
控制传输生命周期。实际鼠标描述符/状态前缀加入夹具后单项补跑通过，
1.990 秒。日志：

- `artifacts/build/amac-0.5.1-complete-tests.log`
- `artifacts/build/amac-0.5.1-real-mouse-regression.log`
- `artifacts/build/amac-p4-rev3-mouse-response.log`
- `artifacts/build/amac-p4-es-mouse-response.log`

实板仍为 P4 v3.2，通过 USB2 COM12 只更新应用区 `0x10000`。
先从 Finder 正常关机，更新工具保存经 ROM 摘要确认的 0.5.0 回退副本；
写入校验及独立 ROM MD5 均通过。新应用 SHA-256 为
`7a175d1fd79cc835d2da13314c6f38fdaf4a307aeab32e6f66c7a939a8b64b8e`。
更新记录：`artifacts/updates/2026-11-11_115249_432142/result.json`。
随后自动启动 System 7，USB 控制、真实鼠标移动及桌面应用运行正常。

本次实板捕获明确复现了乱跳根因：连接的 `04f3:026f` 鼠标接口 0 发送
相对位移，接口 1 的端点 `0x82` 约每 4918 ms 发送一次 32 字节状态包。
原始前缀为 `04000401640004000120010000000000`。旧驱动将接口 0 的
布局用于这一包，解码成 `dx=1024, dy=25601, buttons=4`，因此静止时也会
跳动并产生中键。新版将该包留在接口 1，忽略其鼠标输入含义。

更新后的 60 秒空闲采样收到 13 包此类状态报告，接受的鼠标位移报告为 0，
USB 传输错误为 0，最大 USB 服务间隔 2312 µs。随后实际移动鼠标的累计
接受报告超过 21000，错误仍为 0，最大服务间隔 2520 µs；短时采样中连续
真实报告的时间戳间隔中位数为 2 ms。这些数值衡量 USB 接收服务，不能
直接等同于屏幕端到端延迟；主观跟手程度仍需用户确认。

证据位于 `artifacts/mouse/2026-11-11/`：

- `boot-and-input.log`、`update.log`
- `live-input-samples.json`、`sampling-summary.json`
- `active-input-samples.json`、`active-summary.json`
- `after-update-desktop.png`、`before-settings-check.png`
- `final-desktop-status.json`、`release-verification.json`

本轮最后观察到用户正在运行 SimCity，保留了当前游戏状态。0.5.1 启动
设置页的独立实板复测等待正常关机；两处共用的生产接收逻辑已通过上述
测试。0.5.0 的设置页实板验证记录保留在下节。

## 0.5.0：USB 鼠标与不拆机更新（历史记录）

2026-11-11，固件`5.4-beta.1+amac.0.5.0`。固件构建来自干净源码提交
`f9b136f295cde6ba155ec296997e425e0fcb389b`，两份JSON均为
`source_dirty=false`。后续PC更新工具的原生USB复位、自动归档及说明提交
不改变这两份固件。标签`amac-v0.5.0-usb-maintenance`。

| 环境 | 最终构建 | 最低芯片revision | 合并镜像字节数 |
| --- | --- | --- | ---: |
| `amac_p4_rev3` | SUCCESS，20.518秒 | 3.1 | 1952560 |
| `amac_p4_es` | SUCCESS，20.429秒 | 0.1 | 1949792 |

```text
aMac-P4C5-HD20034-Rev3.bin
8140780d9e7534885d1e38b153f15b3808c534efacc0286e29a41d5cb798b783

aMac-P4C5-HD20034-ES.bin
597674019613e810ef9a180e4efd8e193f921f3e2b8a8316b5f914b67e75399e
```

完整88项回归全部通过，123.078秒，支持的C++检查使用ASan/UBSan。
新增生产文件后端与Python客户端的二进制往返、错误CRC/偏移、取消传输、
磁盘读回损坏、替换日志恢复和路径限制检查；启动指针覆盖裁切、显示失败重试
及移除后的底图恢复。固件更新检查拒绝损坏镜像、错误芯片与未提交构建。
日志为`artifacts/build/amac-0.5-complete-tests-final.log`。
另将传输往返用例扩展至196869字节，跨越多个64 KiB读回校验窗口；5项文件
维护检查补跑通过，1.291秒，日志为`amac-0.5-large-file-transfer-tests.log`。

实板为此前同一P4 v3.2。已用新增USB2更新工具只写应用区`0x10000`，
保留SD、NVS、分区表与bootloader。更新前的程序先用ROM Flash摘要匹配已
归档发布镜像，再保存可回退副本；写入校验及独立ROM MD5均通过。
最终应用SHA-256为
`d54dd51723940488302f7634602ce62f8cff00e265613db92b94b9e508deb2fb`。
更新记录为`artifacts/updates/2026-11-11_025423_543734/result.json`。

板上已通过以下检查：

- USB鼠标在设置页初始化，进入Mac复用同一Host。早期实测日志明确报HS hub
  下FS设备不支持TT，改为同一HS控制器的FS/LS根端口后，鼠标正常枚举，解析
  5按钮、16位X/Y、8位滚轮报告，并持续收到中断输入（单次运行计数达到75包）。
- 4 MHz/1-bit SD挂载3840 MB卡，ROM和系统盘读取成功；原20 MHz/4-bit的
  配置写超时消失，`boot-save`通过。
- USB2读取目录与启动配置、上传1880字节诊断文件、重新下载逐字节核对通过。
  再替换为2279字节文件，当前文件及`.previous`旧文件均下载核对通过。
- System 7进入Finder桌面，For You及Start Here图标可见；串口输入通过
  Finder的Special菜单完成正常关机，随后重新进入维护设置、替换文件并再次
  启动Mac。重新挂盘日志为`Volume OK`，XPRAM也正常保存/恢复。
- Mac运行时，SD维护命令及固件更新器均拒绝操作，没有强制复位运行中的Mac。
- 从下载模式用看门狗复位曾出现bootloader异常并进入恢复页。PC工具改用
  原生USB RTS复位后，25秒串口记录无panic/恢复页，自动启动Mac通过。
- 构建脚本自动归档原镜像及JSON，并校验归档哈希。两环境再次运行成功，
  发布二进制哈希保持不变，日志为`amac-p4-*-archive-verified.log`。

文件维护最初发现的loopTask栈保护异常已修正：4096字节读回缓冲改为对象
存储，生产目标的`SdMaintenance::execute`栈帧降为1200字节；随后目录、
上传、校验、替换、备份及启动实测均无该异常。复位/GPIO初始化顺序、ADC
通道首次配置以及生产芯片400 MHz设置告警也已修正。

实板证据位于`artifacts/maintenance/2026-11-11/`：

- `new-firmware-boot.log`、`hardware-maintenance-result.json`
- `replacement-and-restart-result.json`、`maintenance-reentry-uart.log`
- `native-rts-reset.log`、`native-reset-result.json`
- `updater-running-guest-guard.log`、`final-desktop.png`

诊断文件`AMAC_USB_TEST.TXT`及其`.previous`留在SD根目录，不修改Mac盘内
文件。截屏来自实板帧缓冲，并非相机拍摄；面板实际色彩、亮度、电流、实物
断电中断更新、电池阈值/深睡电流及C5联网仍不属于本次硬件验收范围。

## 2026-11-10：实板烧录与 SD 卡准备

已通过 ESP32 内部 USB Serial/JTAG（本机 COM12）烧录现有 0.4.0 Rev3
合并镜像，偏移为 `0x0`。实际芯片为 ESP32-P4 revision v3.2，Flash 16 MB，
PSRAM 32 MiB；固件串口输出版本 `5.4-beta.1+amac.0.4.0`。
镜像、源码提交和哈希仍采用下方 0.4.0 记录，本次没有改变固件。

使用独立安装的 esptool 5.3.1；旧 5.2.0 stub 在该芯片上不能正常运行。
写入前保存原 Flash 前 2 MiB，SHA-256 为
`4bda3a28f4ffe603c0ec1258c0034d65a1a0d35ab7bd523a834608adabf03cc5`。
写入时的哈希校验通过，随后用无 stub 的 ROM 命令独立核对整个写入区域，
MD5 为 `8b7f164c0c7761db3e733a0856b9e5e0`，与镜像匹配。
整段流式读回曾中断，因此不把它记录为完整读回 SHA-256 验证通过。
未修改 eFuse，未执行整片擦除。

首次串口启动记录到 SD 初始化 `0x107` 超时，随后报告找不到 SD/ROM，
尚未进入 Mac。复位过程中还出现过一次 bootloader Illegal instruction 后
看门狗重启；不能据此宣称冷启动稳定性已经验证。CPU 360 MHz 设置失败及
GPIO/ADC 初始化告警仍需后续排查，屏幕、键鼠和电源硬件也尚未完成验证。

已准备用户连接的约 4 GB USB 读卡器中的 SD 卡（当时为 F:、Disk 2）。
保留现有 MBR/FAT32 和原有文件，没有格式化或重新分区。电脑模拟器先正常
关机，确认 HFS 正常卸载并持有会话锁后，复制带生日图标的 System 7 磁盘。
卡根目录文件如下，复制后逐项重新读取并核对 SHA-256：

| 文件 | 字节数 | SHA-256 |
| --- | ---: | --- |
| `Q650.ROM` | 1048576 | `05ad753fb594e656cf078023ec189e09e2a7655a780de993b75b8c51ed6b09ca` |
| `Macintosh.dsk` | 1048576000 | `8b5d82d6ddab6953f1476be2f3517d8e3e88794f0c766141e42b5fcddf937e9a` |
| `basilisk_settings.txt` | 137 | `e78dedb9f1b9e7af5d52e26834df1f6da548120026a1c32b5c5e13d3d9110ce8` |

配置采用 16 MB Mac RAM、0 度方向、直接启动 `/Macintosh.dsk`，关闭声音和
自动 Wi-Fi。只读 `chkdsk` 检查通过，剩余空间 2968354816 字节。
读卡器已通过 Windows 安全弹出。21:31 设备重新连接后仍报告 SD 识别超时，
21:35 再次连接后已成功初始化 SD 卡（3840 MB），找到 `Q650.ROM` 和
`Macintosh.dsk`，并读入 16 MB RAM、直接启动、关闭声音/自动 Wi-Fi 的配置。
已核对当前固件的 slot 0、GPIO39..44、固定 3.3V 配置及网表中五路 10K
上拉对应关系，未发现引脚定义差异。

本次因 reset reason 7 停在恢复设置页，串口控制协议可用，`boot-status` 返回
`preboot/settings`。执行 `boot-start` 返回 `ERR BOOT save_failed`，因此尚未
开始 Mac 模拟器，板上写卡与完整 Mac 启动仍未通过。随后设备连接断开，
继续采集写卡错误需重新连接供电及 USB1。日志中的 `DiskTools1.img: MISSING`
是未放入可选工具软盘，当前启动介质为 `Macintosh.dsk`。

本地证据（不纳入 Git，ROM 和客户机磁盘也不纳入 Git）：

- `artifacts/flash/2026-11-10/usbjtag-flash-session.log`
- `artifacts/flash/2026-11-10/usbjtag-rom-verify-boot.log`
- `artifacts/flash/2026-11-10/usbjtag-run-001/result.json`、`boot.log`
- `artifacts/sdcard/2026-11-10/preparation.json`、`preparation.log`
- `artifacts/sdcard/2026-11-10/chkdsk.log`、`ejection-reader.json`
- `artifacts/sdcard/2026-11-10/device-boot.log`

## 0.4.0 桌面与系统入口（历史记录）

2026-11-09，版本`5.4-beta.1+amac.0.4.0`。生日礼物改用桌面For You图标，
增加控制面板Birthday Settings、苹果菜单Pocket Terminal和About This Gift。
四个原生68K启动器及Start Here说明已加入本地两份HFS磁盘；原盘备份位于
`artifacts/desktop/backup/before-birthday-icons/`。固件与电脑端共用彩蛋逻辑和绘图。

最终从干净源码提交`2ce37064e82e0314dcf4c38239841c6d55646e5e`构建，
两份JSON均为`source_dirty=false`。本次验证提交只更新文档，版本标签为
`amac-v0.4.0-desktop-gifts`；此前0.3.0两份镜像及JSON已归档至
`release/archive/amac-v0.3.0-birthday/`，复制后逐项核对SHA-256。

| 环境 | 构建结果 | SDK最低芯片revision | 完整镜像字节数 |
| --- | --- | --- | --- |
| `amac_p4_rev3` | SUCCESS，49.671秒 | 3.1 | 1939968 |
| `amac_p4_es` | SUCCESS，49.819秒 | 0.1 | 1937200 |

```text
aMac-P4C5-HD20034-Rev3.bin
7a813cb00c94dad742f74cbba813c80b9871dac914505585e7e236a9325e498a

aMac-P4C5-HD20034-ES.bin
a9f33f3f0faf55af368ef22962b4014b96409af8f2853f18ed502ac7961a7d88
```

两份产物独立核对了合并偏移、bootloader/partition/application完整内容、
各组件与整包SHA-256、P4芯片ID、SDK最低revision、固件版本、源码提交与
C5标识，并检查链接map未混入SDK的C6 transport。P4预编译镜像头的
minimum-revision字段并不可靠，最低芯片要求取自实际所用SDK的sdkconfig，
与既有构建守卫保持一致。上游/SDK既有编译警告仍存在，构建均成功。

完整测试发现并执行82项，152.316秒，全部通过，未跳过用例。
包括11项aMac、原生MacBinary/SDL桌面、打包/DSI、RTC/电池、输入并发、
旧板型显示/启动界面、68K标志与AOT差分、控制协议及工具回归。
支持的显示/输入和aMac C++主机检查开启ASan/UBSan；CPU标志检查启用UBSan。
新增覆盖四个资源入口、设置/终端/关于探索路径、命令编辑、输入交接与透明合成。
日志为`artifacts/build/amac-0.4-complete-tests.log`，最终构建日志为
`amac-p4-rev3-desktop-gifts-final.log`和`amac-p4-es-desktop-gifts-final.log`，
独立产物核对见`amac-0.4-release-verification-final.log`。

实际System 7中已验证设置回忆、终端help/ls/线索/wish/1984、关于页烟花及返回。
曾发现的WSL脚本换行和Linux loff_t类型冲突已修复，最终完整重跑包含这两项AOT
差分检查，无需跳过。运行中磁盘安装被会话锁拒绝，正常关机后重复安装两盘均
显示五个文件Already current；再次启动复用同一模拟器PID。

System 7及Mac OS 8均通过正常关机，HFS恢复`0x0100`卸载标记。
Mac OS 8从空闲回调直接调用LaunchApplication的测试方式曾使关机卡在内存
管理器；诊断盘/RAM已保存，该自动启动入口已删除。重新启动、由Finder正常
双击For You后关机通过。最终桌面适配层中，Mac OS 8的苹果菜单终端、关于礼物，
以及Control Panels子菜单中的Birthday Settings均经正常菜单打开，再次关机通过。
日志为`artifacts/desktop/verified/`下的`system7-birthday-shutdown.log`、
`system8-fresh-shutdown.log`、`system8-finder-gift-shutdown.log`及
`system8-menu-gifts-shutdown.log`。桌面Start Here已在SimpleText中打开核对。

System 7本地测试盘的桌面图标布局曾出现异常，已重新导入同一For You应用
恢复显示；随后从桌面正常打开、关机、重启，复核图标仍可见。
最终Finder路径日志为`system7-finder-gift-reboot.log`，与前述仅验证资源加载的
自动启动记录区分。所有磁盘操作都在会话锁内、模拟器正常退出后完成。

桌面版同一固件状态机/绘图加SDL2适配，在真正的System 7与Mac OS 8中运行，
未模拟ESP32电源外设。最终桌面程序SHA-256为
`99c108f348911345d6a873c46ddc9fb96a12c878fd95729d38c8c30c26ac746b`。
上述为2026-11-09电脑验证结果；后续烧录与SD准备见顶部2026-11-10记录。
屏幕、USB唤醒、ADC及深睡电流不属于电脑模拟验证范围。

## 0.3.0 生日礼物彩蛋（历史记录）

日期为2026-11-09（Asia/Taipei），固件版本`5.4-beta.1+amac.0.3.0`。
两份镜像来自干净的源码提交`a0075946a3c3b603eb6f635959f52362288b7dc9`，
JSON均为`source_dirty=false`。之后的验证提交仅更新本记录，并补齐既有主机
测试所需的`<cstdio>`头文件，不改变固件。标签`amac-v0.3.0-birthday`。

新增5个彩蛋：蛋糕许愿、Friendship OS、口袋宇宙、升级烟花、1984时间胶囊。
启动轻提示和30秒后的一次短暂邀请引导Ctrl + Alt + G；隐藏触发、退出、
进度和个性化说明见[BIRTHDAY_SECRETS.md](BIRTHDAY_SECRETS.md)（剧透）。
本版使用通用英文祝福。没有修改ROM、Mac磁盘映像或RTC设置。

| 环境 | 构建结果 | 最低芯片revision | 完整镜像字节数 |
| --- | --- | --- | --- |
| `amac_p4_rev3` | SUCCESS | 3.1 | 1936544 |
| `amac_p4_es` | SUCCESS | 0.1 | 1933792 |

```text
aMac-P4C5-HD20034-Rev3.bin
7ee98d1f358aaf53de07d607252067d763ffe193ba08c200ea5d37c4aa30b5b0

aMac-P4C5-HD20034-ES.bin
25061c42ab1b17ad35b6595094083cc6fcce920d970fd79d78e86519b267f187
```

24项主机检查通过：11项aMac、8项打包/DSI、4项既有电池/时钟，以及1项
既有共享输入并发回归。C++检查在WSL下启用ASan/UBSan。重点覆盖：

- 5种触发、重复按键、错序/超时、长按移开取消、毫秒计数回绕与超大鼠标位移；
- 打开窗口前释放后台已经按下的键鼠，多个输入源继续保留各自的按键所有权，
  关闭后没有孤立释放或按键粘连；既有并发测试完成60000次输入操作；
- 实际彩蛋绘图按完整画面和不规则小块绘制一致，写入不越界；
- 实际视频管线在1/2/4/8 bit和0/180度旋转下合成一致，后台帧缓冲不被修改，
  无后台写入时动画仍刷新，刷新失败可重试，关闭后显示最新后台内容；
- 待机期间停止彩蛋动画，恢复后重新显示；低电压请求Finder关机前收起窗口。

480×360原生绘图预览位于`artifacts/birthday/`，已人工查看5页和信件行宽。
这些图来自固件同一绘图代码，不是实板照片或桌面Basilisk II截图。

两份镜像均再次独立检查了合并偏移、各组件内容及SHA256、最低revision、
源码提交和C5标识。构建经过固定的C5链接守卫，新增彩蛋没有编译警告；
上游/SDK既有警告仍有输出。两份0.2.0镜像及JSON已复制到
`release/archive/amac-v0.2.0-voltage-sleep/`并逐项核对哈希。

日志：`artifacts/build/amac-birthday-host-tests.log`、
`amac-birthday-final-input-tests.log`、`amac-birthday-input-regression.log`，
以及`artifacts/build/amac-p4-*-birthday.log`。
仍未烧录实板；USB键鼠实际手感、显示及电源硬件验证继续待测。

## 0.2.0 电压触发待机与RTC深睡（历史记录）

日期为2026-11-05（Asia/Taipei）。固件源码提交
`98d3fef591583fa2c73656cf29852ba7f25d171c`，两份构建的工作区均干净，
旁置JSON均为`source_dirty=false`。验证记录提交随后只更新本文件。
版本标签为`amac-v0.2.0-voltage-sleep`。

低于3.90 V持续5秒会熄屏并暂停模拟器，USB键盘/鼠标活动唤醒，唤醒后保留
60秒操作时间。低于3.70 V持续5秒请求Finder关机，等待Mac PowerOff后同步、
卸载SD并关闭屏幕/C5，进入RTC每300秒唤醒检查的深睡。恢复到3.90 V后开机。
RTC保留Mac时钟的亚秒锚点，反复电压检查不会累计截断误差或写入NVS。
原始ADC保护默认启用，向Mac报告百分比的开关继续独立。

| 环境 | 构建结果 | 最低芯片revision | 完整镜像字节数 |
| --- | --- | --- | --- |
| `amac_p4_rev3` | SUCCESS | 3.1 | 1924928 |
| `amac_p4_es` | SUCCESS | 0.1 | 1922176 |

```text
aMac-P4C5-HD20034-Rev3.bin
adb3b46d13dc6bed548a933c74cffb41bc8efb3556e136fab6d7e22f44d9ff27

aMac-P4C5-HD20034-ES.bin
f34ce3c48e6683e3cb0200d3adce75e25345cfb4e0b19bacd4ca0b8ad5d86b18
```

9项aMac检查、8项打包/DSI检查和4项既有电源/时钟检查通过，共21项。新增覆盖
3.70/3.90 V边界、连续低电压确认、噪声/无效采样、键鼠唤醒宽限期、深睡优先级、
毫秒计数回绕、默认关闭百分比时的ADC路径、反复深睡后RTC的秒与亚秒计时，
以及面板休眠、失败恢复时重新绑定不同帧缓冲。既有电源/时钟四项在WSL下开启
ASan/UBSan，其余aMac原生检查使用Windows G++。

生产`mac_shutdown.cpp`还在桌面Basilisk II中运行，分别启动System 7和Mac OS 8
的独立磁盘副本，通过同一个SynchIdleTime入口发送`MACS`目标、`FNDR/shut`
事件。两次进程均正常退出且返回0，两份HFS卷头均恢复`0x0100`正常卸载标记。
日志位于`artifacts/sleep/mac7/shutdown.log`和`artifacts/sleep/mac8/shutdown.log`。

两份合并镜像再次独立核对了bootloader、partition和application内容、芯片ID、
最低revision、全部SHA-256及构建源码提交。构建同时通过C5链接守卫。
日志在`artifacts/build/amac-p4-*-low-battery.log`和
`artifacts/build/amac-low-battery-*-tests.log`。

尚未烧录实板。实际ADC误差、USB键鼠唤醒、深睡/唤醒、RTC漂移和整机电流
待测。早期工程样片数字GPIO保持有DIG-399限制，C5复位保持也需要实测。
保存文档提示会等待用户处理后继续关机。深睡后Mac重新启动。

## 0.1.1 补充附件屏幕配置（历史记录）

日期：2026-11-03（Asia/Taipei）。代码提交
`ac4dc4de427edd3a97dd5296423b1ea72357b21f`，两种固件构建时工作区干净，
`source_dirty=false`。版本标签：`amac-v0.1.1-kernel-display`。
后续验证记录提交只更新本文件。

采用 `kernel修改说明.zip` 的屏幕 DTSI：41条DCS初始化、每条命令延时、
240 Mbps/lane、水平48/43/10和垂直18/28/10的FP/BP/SYNC、200 ms复位保持和
120 ms复位释放等待。像素时钟15 MHz，理论约62.06 Hz。帧缓冲和DSI输出
保留RGB565，对应COLMOD=0x55；附件Linux RGB888格式差异见
[屏幕驱动参考](hardware/reference/HD20034_KERNEL_REFERENCE.md)。

| 环境 | 构建结果 | 最低芯片revision | 完整镜像字节数 |
|---|---|---|---|
| `amac_p4_rev3` | SUCCESS | 3.1 | 1908112 |
| `amac_p4_es` | SUCCESS | 0.1 | 1907120 |

```text
aMac-P4C5-HD20034-Rev3.bin
c27b57586c7db4d727a6fd3637b6191cca25095fb8d29d330d396905f63cc755

aMac-P4C5-HD20034-ES.bin
023273c79a0e8d1fa8948361211f66972e610b1a1f7cccca18000e0ac8b4e391
```

验证结果：7项aMac主机测试和8项打包/DSI补丁回归检查通过，共15项。
新增检查逐字节比较41条DCS记录（含包类型、长度、数据、延时），并运行生产
`board_display_amac.cpp` 的上电流程，检查时序、200/120 ms复位、背光开启
顺序、失败资源清理和重复初始化。已有三种板型显示/UI回归继续通过。
构建后的两份合并镜像、旁置JSON哈希、源码提交/dirty状态及C5链接map再次
独立校验通过。原生测试使用G++，未启用ASan。

本地日志：

- `artifacts/build/amac-p4-rev3-kernel-display.log`
- `artifacts/build/amac-p4-es-kernel-display.log`
- `artifacts/build/amac-kernel-display-tests.log`
- `artifacts/build/amac-kernel-display-packaging-tests.log`

旧二进制/JSON在 `release/archive/amac-v0.1.0-reference/`，已核对其SHA256。
Windows构建的C5私有头文件路径已限定于对应源文件，解决WiFi编译命令超过
系统长度限制的问题。

仍未烧录实板。新的初始化、RGB565格式适配、实际刷新率和面板点亮尚待上板；
以下首版硬件待测项目继续适用。

## 0.1.0：通用参考配置（历史记录）

日期：2026-11-03（Asia/Taipei）。移植代码基于上游提交
`3641580d72982cec464d4800f2ead507ba7c687d`。

## 已完成

- 生产芯片配置 `amac_p4_rev3` 已完成编译、链接和合并镜像校验。
- 链接 map 已确认使用 aMac 的源码 C5 transport，未采用 SDK 的 C6 transport。
- 原生显示/界面测试：aMac、Tab5、Waveshare 三种配置，1/2/4/8 bit 色深、
  旋转、失败重试、生命周期和界面像素一致性通过。
- 固件合并及 DSI 初始化补丁相关的8项主机回归测试通过。
- 界面截图已人工查看：480×360 设置、Wi-Fi、启动画面；控件不越界。

## 最终构建与产物

两种镜像均由干净工作区的源码提交
`581adbaaf9c17e3ce787aab60cf7668db0913a62` 构建，`source_dirty=false`。
后续记录提交只更新本文件，不改变固件源码。

| 环境 | 结果 | SDK 最低 revision | 完整镜像字节数 |
|---|---|---|---|
| `amac_p4_rev3` | SUCCESS | 3.1（301） | 1907296 |
| `amac_p4_es` | SUCCESS | 0.1（1） | 1906304 |

SHA256：

```text
aMac-P4C5-HD20034-Rev3.bin
741c457a98b94adeae98f3d36c2bf643c0a98498fdf845291bb4ea2f24ed0b41

aMac-P4C5-HD20034-ES.bin
5e46eb5109c6796ae40b08d0d3ae790676d77010b8addad360ef19d6eb2037f0
```

镜像旁 JSON 记录相同哈希、源码提交、芯片要求、各组成部分校验和与 C5
transport 版本。两份合并镜像已再次独立逐字节校验 bootloader、partition 和
application，并通过芯片 ID、链接 map 检查。

主机验证：5项 aMac 测试通过，其中显示管线与启动 UI 分别覆盖 aMac、Tab5、
Waveshare；另有8项合并镜像/DSI 补丁回归检查通过，共13项。G++ 15.2 原生
运行，没有启用 ASan。检查范围包括全部支持的索引色深、0/180度旋转、缓存
发布失败重试、并发写入及生命周期。最终480×360设置画面已确认隐藏无音频和
USB Disk 控件，所有可用控件均在屏幕内。

本机构建/测试日志（忽略构建产物，不纳入 Git）：

- `artifacts/build/amac-p4-rev3.log`
- `artifacts/build/amac-p4-es.log`
- `artifacts/build/amac-host-tests.log`
- `artifacts/build/amac-packaging-tests.log`

原生界面图片：`artifacts/amac-ui/BOARD_AMAC_P4/`。
Git 版本标签：`amac-v0.1.0-reference`。上游基线与全部移植提交均保留。

## 需要实机完成

尚未连接开发板或执行烧录，不能以编译/主机截图代替面板点亮和 Mac OS 启动。

| 项目 | 当前状态 |
|---|---|
| 芯片真实 revision、Flash/PSRAM 型号 | 待读取 |
| 排线方向与电源/地导通 | 待实测 |
| ST7701 LCD ID、参考初始化、刷新时序/颜色 | 待上板 |
| 背光极性、模拟亮度与电流 | 待上板 |
| SD 连续读写与 Mac OS 启动 | 待上板 |
| USB hub 后键盘/鼠标、热插拔 | 待上板 |
| C5 2.12.9 与 host 2.12.3 联网、SD 并发 | 待上板 |
| ADC 校准、实际电池参数 | 待确认；默认禁用电量估计 |

屏幕采用本次确认的乐鑫参考初始化，尚没有该型号厂商专用寄存器表。
