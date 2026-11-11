# System 7 生日壁纸

用户提供的 `E:/AMac/wallpaper.png`，480×360。原图及其署名完整保留在
`wallpaper.png`；`Wallpaper.gif` 是供经典 Mac 使用的 256 色版本。
用 Pillow 的 `MEDIANCUT` 量化，不缩放、不裁切。GIF 实际使用 254 色，
与原图相比三个通道的平均绝对误差分别为 0.211、0.199、0.254（0–255）。
图像作为本项目个人礼物的用户素材使用，作者署名见原图；不因加入仓库
而改变图像本身的权利归属。

## 实板安装

当前实板使用 System 7，固件为 `5.4-beta.1+amac.0.5.1`。
使用 François Pottier 的 [Décor 3.0.4](https://pauillac.inria.fr/~fpottier/decor.html.en)。
作者页面声明该工具现已免费，原安装包中的旧收费注册说明可以忽略。
第三方程序保存在本机安装归档中，没有随仓库分发。

1. 从作者页面取得英文安装包。在 Mac 安装器中选择 **Custom Install →
   Universal Install**，安装至 `MicroMac7` 系统盘。
2. 将 `Wallpaper.gif` 复制到 Mac 硬盘根目录：`MicroMac7:Wallpaper.gif`。
3. 打开 `MicroMac7:Décor ƒ:Décor`，点 **Change…**，选择上述硬盘文件，
   将开关设为 **On**。
4. Options 中保留 Dithering、Scaling、Scale down only、Keep proportions；
   关闭定时换图、Tiling、Scale and crop。
5. 退出 Décor 界面。安装的 `System Folder:Extensions:Décor Daemon`
   在 Mac 启动时加载已保存的壁纸设置。

本板第一次使用 Easy Install 后，主程序的资源头与资源表不一致，启动
报 `-199`。直接覆盖做 Universal Install 只更新了 Daemon；将这份新装的
主程序移至废纸篓后再次 Universal Install，主程序正常打开。重装时保留了
原系统、生日应用与用户文件，未清空废纸篓。

安装介质通过 USB2 的文件维护接口传输，完整 CRC32 校验通过。临时
`Wallpaper.iso` 留在 SD 根目录供维护使用，安装完成后取消 CD-ROM 挂载，
启动设置逐项与安装前副本比较一致。壁纸图片及设置均保存在 Mac 系统盘中。

## 文件校验与记录

| 文件 | SHA-256 |
| --- | --- |
| 原图 PNG | `fb3ffe218d52c5c5bae0747b3b9fc846b447dcb511c0dfafb1f4ec047cdbf31c` |
| Mac GIF | `84a880365958e3d7d3557c0936fffd85a1963d4935f49e31f1a890741c794a24` |
| 作者 Décor 安装包 | `d8dc07c41d0c36e09f3b4c68709e7e82950ab3cc4fdf13480d5d5213de9fcf36` |

安装证据及工具归档：`artifacts/wallpaper/2026-11-11/`。其中包含原启动配置、
格式转换误差、安装盘哈希/传输结果、程序资源头诊断和实板彩色截屏。
这些操作只修改 Mac 客户机内的文件与偏好设置，固件仍采用已发布的 0.5.1。
电脑端预览使用另一份磁盘；可按以上步骤安装同一 GIF。

2026-11-11 实板验证：取消 CD-ROM 挂载、正常关机并重新启动后，壁纸自动
恢复，For You 与 Start Here 图标保留。最终彩色截屏为
`wallpaper-after-restart.png`；启动配置比较结果为 `restored-settings.json`。
