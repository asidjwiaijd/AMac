# aMac 验证记录

## 0.1.1：补充附件屏幕配置（当前版本）

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
