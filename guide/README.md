# 新手指南的插图与生成方式

正文为根目录的 [AMAC_USER_GUIDE.md](../AMAC_USER_GUIDE.md)，可直接在 GitHub
阅读。排版后的 23 页版本为 [aMac_User_Guide.pdf](../output/pdf/aMac_User_Guide.pdf)。
PDF 有目录、书签、页码和可点击的延伸阅读链接，彩蛋只给线索。

## 插图来源

| 文件 | 来源 |
| --- | --- |
| desktop.png | 0.6.3 网络校时完成后的实机彩色桌面 |
| startup-settings.png | 实机 Startup Settings 示例，拍摄时为旧内存/时间设置 |
| wifi-example.png | 同套 aMac 界面的演示渲染，网络名和地址为示例 |
| password-keyboard-example.png | 同套界面的演示渲染，密码为示例 |
| date-time.png | 修复时钟写入后的实机 Date & Time 控制面板 |
| shutdown-menu.png | 实机 Special → Shut Down 菜单 |
| birthday-cake.png | 生日应用的公开起始页面，仅展示明显入口 |
| desktop-controller.png | 本次构建的独立 Windows 控制台连接实机、正在捕获时的窗口 |
| connections.png | 原生绘制的接口功能示意，不代表实物布局 |
| storage.png | 原生绘制的 SD 与 Mac 虚拟硬盘层次图 |
| power-flow.png | 原生绘制的关机、RTC 检查与充电唤醒示意 |

截图未裁切或修改内容。壁纸为用户提供的礼物素材，画面原署名保留；
图片权利不因进入本项目而改变，详见[壁纸记录](../guest/wallpaper/README.md)。

## 重新生成

```powershell
python -m pip install -r tools/requirements-guide.txt
python tools/build_user_guide.py
```

生成脚本默认读取 Windows 的 `Deng.ttf` / `Dengb.ttf`，以子集形式嵌入 PDF；
字体文件本身不随仓库分发。其他系统请用 `--font`、`--bold-font` 指向可嵌入的
中文 TrueType 字体。临时绘图位于 `tmp/pdfs/`，不进入 Git。

当前版本已用 Poppler 渲染全部页面并目视检查，另检查书签、链接、文字边界、
图片数量和中文文本提取。修改正文后应重新生成 PDF，并再次检查分页与图注。
