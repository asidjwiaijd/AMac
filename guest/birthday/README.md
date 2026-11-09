# 原生Mac生日应用

四个小型68K应用通过Finder正常打开，加载专用`bDAY/128`资源，再返回Finder。
aMac固件或生日版电脑模拟器识别带版本的资源标记并显示相应礼物页面。
普通Basilisk II没有该适配层，应用会直接退出。

| 应用 | HFS安装位置 | 页面 |
| --- | --- | --- |
| For You | `:Desktop Folder:For You` | 生日蛋糕 |
| Birthday Settings | `:System Folder:Control Panels:Birthday Settings` | 个人设置和回忆线索 |
| Pocket Terminal | `:System Folder:Apple Menu Items:Pocket Terminal` | 探索终端 |
| About This Gift | `:System Folder:Apple Menu Items:About This Gift` | 关于与烟花线索 |
| Start Here | `:Desktop Folder:Start Here` | SimpleText说明文档 |

`python3 guest/birthday/package.py`确定性生成`dist/*.bin` MacBinary II文件，
包含数据叉、资源叉、文件类型、creator、Finder位置和单色自定义图标。
MacBinary不能作为普通文件直接复制到HFS卷，需使用`hcopy -m`解包。

在WSL仓库根目录运行`bash scripts/install_birthday_desktop.sh`，为本地
`artifacts/desktop/Macintosh7.dsk`和`Macintosh8.dsk`安装。必须先在Mac中正常关机；
安装器持有会话锁、备份原盘，并拒绝覆盖无关同名文件。
将安装后的映像复制到实板SD卡，即可配合0.4.0固件使用。

应用的CODE0跳转表入口偏移从CODE1四字节段头之后起算，显式调用
`GetResource('bDAY',128)`，以覆盖Segment Manager内部的快速加载路径。
标记包含`aMacBday`、格式版本1和页面号1至4；识别器核对资源类型、ID和标记，
兼容资源管理器按堆块大小传回的尾部填充。
`test_birthday_desktop.py`独立检查MacBinary CRC、资源映射、段入口与SDL适配，
不需要68K编译器或商业资源编辑器。