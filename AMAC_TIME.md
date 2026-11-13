# aMac 自动网络校时（0.6.3）

开机连接已保存的 Wi-Fi 后，自动向网络时间服务器校时，显示台北时间
（Asia/Taipei，UTC+8）。联网过程和校时均在后台进行，启动不等待网络。
正常每 3 小时复校一次；断线后重新连上 Wi-Fi 会重新请求时间。
无需电脑或拆卸 SD 卡。请在启动设置中保存网络并开启自动连接。

时间服务器依次为 `ntp.aliyun.com`、`time.cloudflare.com`、
`time.windows.com`，使用固定 IDF 5.5.4 的 SNTP 服务。网络暂不可用时，
继续使用现有 RTC 时钟；未曾校时则沿用固件构建时间或保存的时钟检查点。
深睡期间继续保留 RTC 时间。完全断电或 RESET 冷启动会恢复最近保存的
时钟检查点，联网后重新校准，因此不能保证断电期间仍准确计时。

校时结果在主任务应用到 Mac 时钟，并保存到设备配置区。网络回调只缓存
结果，不直接操作 Mac 内存、磁盘或配置区；补偿网络回调到主任务之间的
时间延迟，保留亚秒部分。只接受 2020 年起且未超过 Mac 32 位时钟范围的
样本。离线和校时均不改变用于休眠防抖、输入或超时的单调计时。

可在 Mac 的 Date & Time 控制面板手动调整时间；联网后的自动校时会再次
按台北时间校准。0.6.3 同时修复 ROM 的时钟与 PRAM 写入：从 D1 高字取
实际数据，不再将调用者 D2 中的旧值（SetDateTime 路径为 0x3a）写成时间。
时区和日期设置因此也可正常保存。

电脑端打开 `scripts/maintain_amac.cmd`，选择 **9 时间同步状态**，可查看
当前时间、是否联网及距上次校时的秒数。诊断协议 `TIME` / `TIME STATUS`
在启动设置和 Mac 运行阶段均可用，返回 `syncs`、`utc`、`age_s`、`mac`、
`local`、`tz` 等字段；尚未收到网络样本时 `syncs=0`、`age_s=-1`。

SDK 接口依据：[乐鑫 P4 系统时间说明](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/system_time.html)。
当前构建的 `CONFIG_LWIP_SNTP_UPDATE_DELAY=10800000` ms，最多三台服务器，
首次请求可随机延迟最多 5 秒。网络重试由 SNTP 管理，初始化失败最多每
30 秒重试一次。实板结果另记于[验证记录](AMAC_VALIDATION.md)。
