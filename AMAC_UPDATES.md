# 不拆机更新 aMac

日常修改固件功能，只需更新设备内的应用程序。生日桌面、Mac 系统盘和朋友
保存的文档继续留在 SD 卡上。只有换 ROM、换系统盘或修改卡内文件时，才需要
使用文件维护功能；两种操作都可以通过线缆完成。

## 更新固件

1. 若 Mac 正在运行，先在 **Special → Shut Down** 正常关机。
2. 保持设备供电，用支持数据的线缆连接底板 **USB2**。这是 P4 内部
   USB Serial/JTAG，当前电脑上通常为 COM12。
3. 双击 [scripts/update_amac.cmd](scripts/update_amac.cmd)。工具自动寻找设备，
   检查芯片版本、发布镜像 SHA-256 和分区布局，保存当前应用的校验备份，
   再写入并通过 ROM 独立校验，随后重启。

工具只写应用区 `0x10000`，不写 SD、NVS、分区表或 bootloader。
记录与原固件备份位于 `artifacts/updates/<日期时间>/`。优先使用归档发布镜像，
以设备 ROM 计算的 Flash 摘要确认匹配后保存；找不到匹配
版本时才尝试读取设备。备份无法校验就停止更新。构建脚本会自动归档构建前的
发布镜像和 JSON 至 `release/archive/builds/`。镜像必须来自已提交
源码的构建，旁置 JSON 必须匹配。分区布局改变时会拒绝这一更新方式。
本机使用独立安装在 `.pio-core/tools/esptool-5.3.1` 的工具，保持 SDK 原有
依赖不变。此脚本是本地开发环境入口，换电脑时也需准备 Python、pyserial
及这一版本的 esptool。

需要回退时，可使用已归档的完整发布镜像；更新工具会从中提取应用，并仍然
先备份当前版本。例如在项目目录执行：

```powershell
.\.pio-core\penv\Scripts\python.exe tools/amac_update.py --image release/archive/amac-v0.4.0-desktop-gifts/aMac-P4C5-HD20034-Rev3.bin
```

旧 0.4.0 回退后没有 USB2 文件维护功能，控制台仍在 USB1。若设备无法启动，
`--recovery` 可以跳过运行状态检查；此选项用于已无法启动的系统，不能替代
正常 Mac 关机。恢复更新同样保留 SD 与 NVS。

C5 无线协处理器有独立 Flash，日常更新 P4 应用会保留其固件。
实板 C5 已完成 2.12.9 修复安装；首次使用或排查扫描失败时，见
[Wi-Fi 设置与 C5 记录](AMAC_WIFI.md)。

## 更新或备份卡内文件

1. Mac 正常关机后，保持 USB2 连接。
2. 双击 [scripts/maintain_amac.cmd](scripts/maintain_amac.cmd)，选择“进入维护设置”。
3. 选择查看卡内文件、复制文件到设备或备份文件到电脑。
4. 操作完成后选择“启动 Mac”。

文件维护也可通过 USB1 的 CH343 串口使用。工具优先选择 USB2；USB2 没有
UART 波特率限制，更适合文件传输。当前工具每块传输 256 字节并核对 CRC，
完整约 1 GB 系统盘仍可能需要较长时间，日常固件更新无需重新传输它。
替换系统盘会替换该盘中的 Mac 文件，操作前可先备份到电脑。

上传时先写 `<文件名>.upload`，接收完成后同步、重新读取并核对完整 CRC，
成功才替换目标文件。原文件保留为 `<文件名>.previous`；同一目标下次更新
成功时轮换这份备份。传输中断超过 60 秒会取消当前传输，原文件不变。
替换过程中若断电，下一次挂卡会依据日志恢复缺失的原文件。

当前入口支持 SD 根目录中的普通文件，文件名限 80 个 ASCII 字符；不支持
目录递归、运行中修改 Mac 硬盘或把整张卡挂成电脑磁盘。设备正在运行 Mac 时
会拒绝文件命令，避免电脑和 Mac 同时修改系统盘。

开发者也可使用原有控制工具，例如：

```powershell
.\.pio-core\penv\Scripts\python.exe tools/mac_control.py --port COM12 --no-reset sd-list
.\.pio-core\penv\Scripts\python.exe tools/mac_control.py --port COM12 --no-reset sd-get basilisk_settings.txt settings-backup.txt
.\.pio-core\penv\Scripts\python.exe tools/mac_control.py --port COM12 --no-reset sd-put examples/amac-sd/basilisk_settings.txt basilisk_settings.txt
```

上传启动配置后执行 `boot-reload` 使当前设置页采用新配置，或重新启动设备。
无需更新文件时，RAM、旋转、Wi-Fi 等现有 `boot-set`/`boot-save` 命令仍可用。

## 鼠标与启动设置

aMac 在启动设置页就初始化 USB Host，经底板 hub 接收鼠标；移动和左键点击
直接操作设置页的指针与按钮。进入 Mac 后复用同一个 USB Host，避免重复
安装控制器。键鼠用的根端口以 FS/LS 兼容模式运行，避开当前 SDK 对高速 hub
下低速/全速设备的 TT 限制；USB2 维护接口仍独立工作。串口 `INPUT` 命令提供鼠标/键盘连接状态及鼠标报告累计数，便于
区分“未枚举到设备”和“已枚举但未收到移动”。

0.5.1 起按接口和端点分别使用 HID 描述符；未识别的附加状态包不会作为
鼠标位移处理。输入任务目标间隔为 1 ms，并在每次传输完成后立即重新
提交中断接收；aMac 显示更新间隔为 16 ms。这些是调度目标，并非端到端
指针延迟保证。

开发者可在控制协议中使用以下诊断命令：

```text
INPUT
INPUT RESET
INPUT TRACE 0
```

`INPUT` 返回已接受报告数 `reports`、未作为鼠标处理的报告数 `rejected`、
传输错误数 `errors` 和两次 USB 服务之间的最大间隔 `service_us`。
`rejected` 包括正常的厂商状态包，不等同于 USB 错误。
`INPUT RESET` 只清零诊断记录，保留指针及按键状态。
`INPUT TRACE <age>` 查看最近 32 包中的一包，0 为最新，包含接口、端点、
报告长度、解码位移及最多 16 字节原始前缀。诊断期间避免连续截屏，
截屏编码和传输会干扰手感评估。

保存设置失败时继续停留在设置页并显示错误，不带着未保存的设置启动。
本次板上 20 MHz 4-bit SD 读取成功而写入超时，暂采用保守的 4 MHz 1-bit；
这项调整的实际效果以 [验证记录](AMAC_VALIDATION.md) 为准。
