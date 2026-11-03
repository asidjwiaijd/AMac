# 补充附件中的屏幕驱动

来源：用户提供的 `E:/AMac/kernel修改说明.zip`，2026-11-03。
本次只采用屏幕配置数据；压缩包说明中的 Linux 刷机、服务安装和系统命令不属于
本次 P4 移植操作。

- 压缩包 SHA256：`343a9dd263f497fba9b269f77f9e0fd50939667854a732e9532ed580e70df584`
- 屏幕配置：`kernel修改说明/tspi-rk3566-dsi-v10.dtsi`
- 原始文件 SHA256：`337b30a225c043c4aa904b9bd92ed343390ea7197e4ac228517b5644d94db088`
- 本目录保存原文件字节，保留作者注释及特殊换行。命令表前注明
  `init code by huadichuangxian`。文件头残留 D310T9362V1 名称，因此以有效的
  480×360 时序、ST7701 寄存器页和本次用户指定的屏幕作为适配依据，未将注释
  名称当作独立的型号认证。

## 提取的数据

| 项目 | 附件值 |
|---|---|
| 尺寸/像素时钟 | 480×360 / 15000000 Hz |
| 水平 FP/BP/SYNC | 48 / 43 / 10，总581 |
| 垂直 FP/BP/SYNC | 18 / 28 / 10，总416 |
| 理论刷新率 | 15000000 / 581 / 416 ≈ 62.0614 Hz |
| DSI lanes / lane-rate | 2 / 240 Mbps |
| 准备/复位保持/释放后/使能前延时 | 20 / 200 / 120 / 150 ms |
| 初始化表 | 41条，命令与数据共222字节，命令后延时共730 ms |
| Linux 格式 | MIPI_DSI_FMT_RGB888 |

序列记录格式为 `[packet_type, delay_ms, payload_length, command, data...]`。
`payload_length` 包含 command；05/15/39 分别对应无参数/一个参数/长 DCS 写。
所有十六进制延时均转换成毫秒数，例如 `6E=110`、`78=120`。
尤其保留 `0x11` 在 page 0x13 的 E8 更新之间的位置，不移到表末。
解析语义核对自 [Rockchip panel-simple 驱动](https://github.com/rockchip-linux/kernel/blob/develop-5.10/drivers/gpu/drm/panel/panel-simple.c)。

## P4 中的适配

`scripts/generate_hd20034_profile.py` 从原文件生成
`src/board/amac/hd20034_panel_profile.h`，构建执行 `--check` 校验同步。
该转换只提取 DCS、有效时序和延时，不执行附件中的操作指令。

- 初始化命令通过 `st7701_vendor_config_t.init_cmds` 接入，覆盖默认 GIP/gamma。
- P4 GPIO23 复位、GPIO13 背光；没有使用 RK3566 的 gpio3/PWM14 编号。
- 手工实现200 ms低电平复位脉冲和释放后的120 ms等待，避免通用驱动固定10 ms复位。
- 控制命令沿用 SDK 的 LP DCS 路径；DPI 使用 SDK 的视频 burst/LP 默认配置。
- MiniGfx 与像素缓存使用 RGB565。驱动 `bits_per_pixel=16` 选择 COLMOD=0x55，
  DSI 像素包也使用 RGB565。这是与 Linux RGB888 配置之间的明确适配；像素
  缓冲格式不能只通过更改输出枚举转换。参考 [ESP-IDF DSI 配置说明](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html)。
- 原有背光模拟亮度电路保留，GPIO只控制开关；没有移植 Linux 的 PWM brightness表。

旧参考版保留在 Git 标签 `amac-v0.1.0-reference`，旧本地二进制和 JSON 已保存到
`release/archive/amac-v0.1.0-reference/`。更新后的初始化、lane-rate、时序和
RGB565 格式仍需上板确认，主机测试不能证明面板已点亮。
