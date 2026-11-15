# aMac 架构图

基于 2026-11-16 的当前源码，固件版本 0.6.4。

| 图 | 交互版 | 图片 | 矢量版 | 编辑源 |
| --- | --- | --- | --- | --- |
| 系统架构 | [HTML](amac-system.html) | [PNG](amac-system.png) | [SVG](amac-system.svg) | [JSON](amac-system.architecture.json) |
| 代码框架 | [HTML](amac-code.html) | [PNG](amac-code.png) | [SVG](amac-code.svg) | [JSON](amac-code.architecture.json) |

模块说明、目录树和运行路径见 [架构正文](../../AMAC_ARCHITECTURE.md)。
交互版可缩放、查看节点说明与源码引用；PNG/SVG 为同一交付图的原生导出。

## 校验

- `*.delivery.json`：图形数据与 HTML 的摘要；每图 9/9 检查，0 错误、0 警告。
- `*.visual-check.json`：四种桌面尺寸的浏览器记录和浅色/深色截图。
- `review.json`：图片检查结论及对应交付摘要。
- `export-receipt.json`：原生 SVG/PNG 的字节数、摘要及来源 HTML 摘要。
- `SHA256SUMS.txt`：本目录交付文件的 SHA-256。

图中文字和连线以 `*.architecture.json` 为编辑源。修改后需重新生成 HTML、
完成图形和浏览器检查，并重新导出图片；不要直接修改生成的 HTML。

本机生成器位于 `%USERPROFILE%/.agents/skills/archify/`，典型检查命令：

```powershell
node "$env:USERPROFILE/.agents/skills/archify/bin/archify.mjs" validate architecture output/architecture/amac-system.architecture.json --quality showcase --repo-root . --json
node "$env:USERPROFILE/.agents/skills/archify/bin/archify.mjs" deliver architecture output/architecture/amac-system.architecture.json output/architecture/amac-system.html --quality showcase --repo-root . --json
node "$env:USERPROFILE/.agents/skills/archify/bin/archify.mjs" visual-check output/architecture/amac-system.html --json
```

代码框架图使用同样命令，将 `amac-system` 改成 `amac-code`。
完整出处对应源码提交 `a3093fea7bf2a31d75045853512cc8bd2e7fadb0`。
