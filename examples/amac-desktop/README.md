# 电脑端预览

双击 [`run_amac_desktop.cmd`](../../scripts/run_amac_desktop.cmd) 启动。
关机时在 Mac 菜单栏选择 **Special → Shut Down**，让 Finder 保存磁盘并正常卸载。
本机已验证 System 7 和 Mac OS 8 都能进入 480×360 的 Finder。启动脚本默认用
System 7。如需启动 Mac OS 8，关机后把本地 `artifacts/desktop/basilisk_ii_prefs`
中的 `disk Macintosh7.dsk` 改为 `disk Macintosh8.dsk`。

## 从干净检出准备文件

在启用图形应用的 WSL Ubuntu 中准备 `build-essential`、`autoconf`、`automake`、
`pkg-config`、`libsdl2-dev`、`libx11-dev`、`libxext-dev`、`hfsutils` 和 `unzip`。
从固件仓库根目录运行以下命令编译桌面版。编译目录在 WSL 文件系统中，产物
放在被 Git 忽略的 `artifacts/desktop`。

```bash
mkdir -p artifacts/desktop
desktop=$(realpath artifacts/desktop)
source_dir=$(mktemp -d)
git clone https://github.com/kanjitalk755/macemu.git "$source_dir/macemu"
git -C "$source_dir/macemu" checkout --detach f532be3d3d70b70f272b52050ce0889a4025d624
cd "$source_dir/macemu/BasiliskII/src/Unix"
sh ./autogen.sh --without-gtk --enable-sdl-video --enable-sdl-audio \
  --disable-jit-compiler --enable-addressing=direct
make -j4
install -m 755 BasiliskII "$desktop/BasiliskII"
```

从 [M5Tab-Macintosh 的 SD 卡包](https://www.mcchord.net/static/sdCard.zip)
提取 `sdCard/Q650.ROM`、`sdCard/Macintosh7.dsk` 和可选的
`sdCard/Macintosh8.dsk`，平放在 `artifacts/desktop`。保留原 ZIP 作为备份。

## 上游镜像一直显示寻盘图标

原包的两份 HFS 镜像均未设置卷头 `drAtrb` 的正常卸载标记 `0x0100`。桌面版
Basilisk II 能读取启动块和目录，但启动停在寻盘。用同一 ROM、同一配置测试，
原始镜像停在寻盘，两份正常卸载的镜像均进入 Finder。另用一份 System 7 副本
只改该标记也能启动。该标记的定义见 [Apple HFS 格式头文件](https://github.com/apple-oss-distributions/hfs/blob/main/core/hfs_format.h)。

对于刚从上述 ZIP 提取的镜像，在启动模拟器之前，用 `hfsutils` 检查系统目录
并让工具正常关闭卷。以下命令在 `artifacts/desktop` 下运行，Mac OS 8 可替换文件名。

```bash
flock -n .session.lock sh -c '
  hmount "$PWD/Macintosh7.dsk" &&
  hls -l ":System Folder" &&
  humount "$PWD/Macintosh7.dsk"
'
```

原包内文件的 SHA-256 可用于提取后、修复前核对来源。

| 文件 | SHA-256 |
| --- | --- |
| `Q650.ROM` | `05ad753fb594e656cf078023ec189e09e2a7655a780de993b75b8c51ed6b09ca` |
| `Macintosh7.dsk` | `37aca18d2ddeadd29851c7fd49b73475da37b3578514066fcf9b745840fdc2f5` |
| `Macintosh8.dsk` | `4de7a954ca15d6c8f4257097e5bb39983ab03afae228c03781dee327d55efcee` |
