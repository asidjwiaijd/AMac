# aMac 硬件基线

本目录冻结此次移植采用的输入；较旧的 10 月网表不参与引脚定义。

- `netlists/Netlist_sch_bottom_2026-11-02.tel`
- `netlists/Netlist_sch_front_2026-11-02-1.tel`
- 核心板资料：本地 `../../esp32-p4c5-core-board`，提交
  `72eec71bc2dfc9ac94ae81fda6e32a6a10c31cca`，OSPTEK，CC BY 4.0。
- `reference/c5-p4-connection.png` 复制自该提交，用于内部 SDIO 引脚。
- 屏幕规格书：项目资料目录 `../../HD20034C40-V1规格书.pdf`，
  SHA256 `7d2c65870d955740ca7d76124adc4ddadd70236020d4a62758c2edb5b1fff5b3`。
  PDF 内型号是 **HD20034C40-H-V1**，ST7701S，480×360，MIPI/RGB。
- 核心板中文指南 SHA256：
  `0725f5a0a094ba2beb3227ff4a17c3511e45ac0d44f4f3d070e735f7cc63a6ad`。

## 编号与排线

网表没有排线组件，因此跨板连线是按对应信号推导的安装约束，不能仅凭
网名假定实体排线接法正确。底板 FPC2 与前板 FPC2：**n -> 32-n**。
前板 FPC1 与屏幕规格书第 10/11 页：**n -> 41-n**。
第一次通电前用实物插座方向和导通测量确认，尤其是电源和地。

| 信号 | P4 / 模组 U3 | 底板 FPC2 | 前板 FPC2 | 前板 FPC1 | 屏幕规格书 pin |
|---|---|---|---|---|---|
| DSI D0+ | 专用 PHY / 38 | 20 | 12 | 35 | 6 |
| DSI D0- | 专用 PHY / 39 | 21 | 11 | 36 | 5 |
| DSI CLK+ | 专用 PHY / 37 | 17 | 15 | 32 | 9 |
| DSI CLK- | 专用 PHY / 36 | 18 | 14 | 33 | 8 |
| DSI D1+ | 专用 PHY / 34 | 14 | 18 | 29 | 12 |
| DSI D1- | 专用 PHY / 35 | 15 | 17 | 30 | 11 |
| LCD reset | GPIO23 / 32，经 R23 | 27 | 5 | 7 | 34 |
| 背光使能 | GPIO13 / 28 | 2 | 30 | Q3/Q1 电路 | - |
| 红外输入 | GPIO12 / 27 | 5 | 27 | IR1.1 | - |
| LCD VCC | 固定 3V3 | 29/30/31 | 3/2/1 | 37 | 4 |
| IM2 / IM1 / IM0 | 固定 1/0/1，MIPI 模式 | - | - | 1/2/3 | 40/39/38 |
| LED A / K | 前板驱动电路 | - | - | 39/40 | 2/1 |

3V3 接到屏幕 VCC；规格书给出的 VCI 范围为 2.5-3.6V。P4 MIPI PHY
另由内部 LDO3 提供 2.5V，二者不是同一电源。背光电流由前板模拟电路和
R3 调节，软件只开关 GPIO13，默认高电平开启；器件型号未写入网表，
极性需要在实板上确认，必要时覆盖 `AMAC_BACKLIGHT_ON_LEVEL`。

## 其它资源

| 功能 | GPIO / 连接 |
|---|---|
| SDMMC slot 0，4-bit | CLK43 CMD44 D0=39 D1=40 D2=41 D3=42 |
| SD 检测 | GPIO45 / U6.9；只作输入，不能使用 EV board 的电源脚定义 |
| C5 SDIO slot 1（模组内部） | CLK18 CMD19 D0=14 D1=15 D2=16 D3=17，复位54 |
| 调试 UART0 | TX37 RX38，经 U2 切换到 USB1/U8 串口桥 |
| USB 高速 Host | U3.48/49 -> U1 hub；沿用上游 HID/hub 驱动 |
| USB2 | U3.50/51，即 GPIO24/25，全速 USB serial/JTAG |
| 外部 UART2 | GPIO46 TX、GPIO47 RX，经 220Ω 到 U12 |
| 扩展接口 | GPIO7/8/9/10，不初始化为触摸或编解码器 I2C |
| 电池检测 | GPIO22，R34/R35 都是100K，ADC 电压乘2 |
| 充电状态 | GPIO21 / U9.1，10K 上拉；没有独立 VBUS 检测 |

网表中的封装型号不能证明具体 USB hub、充电芯片或晶体管型号。
没有触摸控制器、音频编解码器和 I2S 扬声器连接；不沿用 Tab5/Waveshare
的触摸和音频驱动。红外仅配置为输入，没有给定遥控协议或键码表。

P4 的 SD IO 参考电源与卡 VDD 是不同概念。板级初始化把内部 LDO4 固定在
3.3V；SD_MMC 使用外部固定供电模式，不自动切换1.8V。网表的卡电源与
pull-up 都接 VCC_3V3，不能沿用开发板的动态 IO 电压策略。模组内
VDDPST_5 与 LDO4 的连线需随上板测量核对；背景见
[乐鑫 SDMMC IO 电源说明](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/sdmmc_host.html#configuring-voltage-level)。
