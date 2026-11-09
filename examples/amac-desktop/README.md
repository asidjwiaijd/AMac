# 电脑端预览

双击[`run_amac_desktop.cmd`](../../scripts/run_amac_desktop.cmd)启动，浏览器打开
本机预览。它运行真正的System 7，显示480×360的Finder及生日应用。
双击桌面的 **For You** 开始，或阅读 **Start Here**。
完整探索路径见[彩蛋说明](../../BIRTHDAY_SECRETS.md)（含剧透）。

关机用Mac菜单 **Special → Shut Down**，等待保存对话框完成、模拟器退出后再
操作磁盘。开发测试也可向`artifacts/desktop/browser/shutdown.request`写入
单行`shutdown`（LF结尾、无BOM）；适配层在客机安全空闲点请求同一个Finder关机。
不要直接关闭运行中的WSL进程。

默认系统为System 7。如需Mac OS 8，正常关机后把本地
`artifacts/desktop/basilisk_ii_prefs`中的`disk Macintosh7.dsk`改为`disk Macintosh8.dsk`，
再启动。`mag_rate 2`将480×360画面显示为960×720；浏览器根据窗口缩放。

## 从干净检出准备

在WSL Ubuntu中准备`build-essential`、`autoconf`、`automake`、`pkg-config`、
`libsdl2-dev`、`libx11-dev`、`libxext-dev`、`hfsutils`、`unzip`、`xvfb`和`x11-utils`。
从固件仓库根目录运行：

```bash
bash scripts/build_amac_desktop.sh
```

桌面上游固定为[kanjitalk755/macemu](https://github.com/kanjitalk755/macemu)的
`f532be3d3d70b70f272b52050ce0889a4025d624`。构建在WSL缓存目录进行，
补丁核对上游文件哈希后接入固件的彩蛋状态机和绘图代码，输出
`artifacts/desktop/BasiliskII-birthday`。不会把ESP32二进制当作电脑程序运行。

从[M5Tab-Macintosh的SD卡包](https://www.mcchord.net/static/sdCard.zip)提取
`sdCard/Q650.ROM`、`sdCard/Macintosh7.dsk`及可选的`sdCard/Macintosh8.dsk`，
放在`artifacts/desktop`，保留原ZIP。确认模拟器已正常关闭，然后运行：

```bash
bash scripts/install_birthday_desktop.sh
cp examples/amac-desktop/basilisk_ii_prefs artifacts/desktop/basilisk_ii_prefs
```

安装脚本先备份原始磁盘并核对副本，再写入四个应用和一份说明。
同名无关文件或编辑过的说明会保留并报错，重复安装不会重写相同内容。
与启动器共用会话锁，运行中的磁盘不能被安装脚本挂载。
实板SD卡也使用安装后的磁盘映像，复制前须先正常关闭模拟器。

Windows中启动`scripts/start_amac_desktop.ps1`，或直接双击上述cmd。
`-NoBrowser`只启动服务并输出地址。再次运行会复用已有预览。

预览使用项目内解包的Ubuntu官方`x11vnc`、`noVNC`和`websockify`，首次启动
会下载所需deb，不需要修改WSLg。Xvfb提供虚拟显示，远程显示及网页只监听
`127.0.0.1:5903`与`127.0.0.1:6083`。日志在`artifacts/desktop/browser/`。
页面地址为`http://127.0.0.1:6083/vnc.html?autoconnect=1&resize=scale&show_dot=0`。

应用通过Finder桌面或苹果菜单打开，日志记录实际加载的bDAY页面资源。
不要从模拟器的空闲回调直接调用Process Manager启动应用；该测试方式曾导致
Mac OS 8关机收尾失败，已移除。正常Finder路径已通过启动和关机检查。

## 原始镜像停在寻盘画面

原包的两份HFS镜像未设置卷头`drAtrb`正常卸载标记`0x0100`。本机对照中，
原始镜像停在寻盘，正常卸载后均进入Finder。安装脚本通过hfsutils正常挂载、
关闭磁盘，也会修复这一状态。卷头标记定义见
[Apple HFS格式头文件](https://github.com/apple-oss-distributions/hfs/blob/main/core/hfs_format.h)。

以下为原包文件的SHA-256，安装后磁盘内容会变化：

| 文件 | 原始SHA-256 |
| --- | --- |
| `Q650.ROM` | `05ad753fb594e656cf078023ec189e09e2a7655a780de993b75b8c51ed6b09ca` |
| `Macintosh7.dsk` | `37aca18d2ddeadd29851c7fd49b73475da37b3578514066fcf9b745840fdc2f5` |
| `Macintosh8.dsk` | `4de7a954ca15d6c8f4257097e5bb39983ab03afae228c03781dee327d55efcee` |
