# aMac Wi-Fi 设置与 C5 修复记录

P4 通过模组内部 SDIO slot 1 使用 C5 的无线功能，SD 卡使用 slot 0。
实板当前为 P4 固件 `5.4-beta.1+amac.0.5.3`，C5 为 OSPTEK 发布的
ESP-Hosted 2.12.9。P4 固定 SDK 中的 host 为 2.12.3，transport 按 C5
从同版本源码编译；这组版本已经在当前硬件上完成连接和数据传输验证。

## 连接网络

Mac 正常关机后，通过 USB2 运行 `scripts/maintain_amac.cmd`，选择进入
维护设置，再打开 Wi-Fi。扫描后选择网络，输入密码并点 Join。连接成功
才保存网络及密码，并开启自动连接；下一次启动会自动关联。

0.5.2 会区分模块启动失败、扫描失败和没有扫描结果。Scan 可以重试；
扫描最多等待 60 秒，连接最多等待 20 秒。初始化失败时停止继续调用扫描，
保持设置页和 USB 维护可用。重新扫描会清空旧列表，并取消尚未完成的关联。

本次实测通过：扫描、2.4 GHz channel 1 / WPA2 关联、P4 获取 IP、重启后
自动连接、电脑经 Wi-Fi 下载并解码完整 480×360 画面。Mac 桌面、生日
图标及壁纸保留。5 GHz、BLE 和 Mac 客户机浏览器不在本次验证范围内。

## 2026-11-11：C5 无法启动

故障时 P4 连续报告 `sdmmc_init_ocr ... 0x107`、`ESP-Hosted link not yet up`。
切换 USB1 的 SW1 至 C5 后，读到 C5 ROM 反复报告
`invalid header: 0xffffffff`。全片备份进一步确认：C5 Flash 的前 64 KiB
均为 `0xff`，其中应放置 bootloader 的 `0x2000` 没有启动头；不能将模块
厂商提供了固件下载等同于当前实物已经预装该固件。

本次使用[OSPTEK 官方 v2.12.9 发布](https://github.com/osptek/esp32-p4c5-core-board/releases/tag/v2.12.9)
的 `ESP32C5_2.12.9_0X0.bin`，1384640 字节，合并包烧至 C5 的 `0x0`。
该包的 bootloader 位于 `0x2000`，分区表位于 `0xc000`，首个应用位于
`0x20000`。写入前确认 bootloader 和应用的芯片 ID 均为 23（C5）、
内部摘要正确、版本范围支持实物 C5 v1.2。发布页 SHA-256 为：

```text
a9b2eaec9428ba37fc4aec23f16908462215eecef230e61ffab8b44f81c39a73
```

用独立 esptool 5.3.1 先读取 C5 完整 4 MiB Flash，再以目标 Flash MD5
核对备份；确认没有启用 Secure Boot/Flash Encryption 后，才写入合并包。
写入时校验和重新连接 ROM 的独立校验均通过。没有修改 eFuse，未执行整片
擦除。原始全片备份 SHA-256：

```text
2306b600cf4185c453a23ebb00087a38fb4cc81151e255b8bf56225833b2d8e2
```

备份和结果：
`artifacts/wifi/2026-11-11/c5-repair-141434/`。
修复后的 C5 启动日志确认 ESP-IDF 5.5.4、ESP-Hosted 2.12.9、SDIO only，
随后成功处理 P4 请求并关联接入点。

实板烧录时观察到：P4 已初始化无线驱动而仍停在设置页时，C5 自动进入
下载模式失败；先让已正常停止 Mac 的 P4 停在 USB2 ROM 下载模式后，C5
自动下载成功。P4 的 GPIO54 直接连接 C5 EN，前者仍驱动该引脚可能影响
USB1 的复位操作。维护 C5 时应由 P4 释放该引脚，并核对串口确为 C5。
完成后可将 SW1 拨回 P4，恢复 USB1 的 P4 日志/调试用途。

日常 `update_amac.cmd` 只更新 P4 应用；C5 是独立 Flash，保留本次修复。
再次出现扫描问题时，先区分设置页显示的失败原因，并检查对应处理器日志。
