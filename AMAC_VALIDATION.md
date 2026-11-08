# aMac 验证记录

## 0.3.0 生日礼物彩蛋（当前版本）

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
