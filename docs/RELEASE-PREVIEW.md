# 0.1.0-preview.1 — 发布说明 / Release notes

2026-11-08发布的首个公开预览版。

[GitHub Release](https://github.com/gzz312/MotionCompensator/releases/tag/v0.1.0-preview.1) · [Windows x64下载](https://github.com/gzz312/MotionCompensator/releases/download/v0.1.0-preview.1/MotionCompensator-0.1.0-preview.1-win-x64.zip)

## 中文

- 提供分区滚动、屏边滚动和4×4静态点阵，支持独立外观设置、交错排布、中央留空与背景明暗/描边适配。
- 通过外部窗口采集、灰度光流和键鼠/手柄输入预测驱动动态点阵。
- 提供中央准星、中英界面、浅深主题、调试可视化和可选日志。
- 单EXE包含所需非系统依赖、使用说明、项目许可、设计致谢及第三方声明。
- 采用根目录LICENSE.txt中的自定义源码公开与商业集成许可。

发布前检查：以UCRT64 GCC15.2和OpenCV4.12静态库完整编译成功，CTest 17/17通过；已检查版本资源、许可嵌入、包内文件一致性及桌面使用说明显示。

已知限制包括独占全屏遮挡、低纹理/武器遮挡导致运动估计不稳定，以及多显示器、高DPI和键盘导航适配不完整。详见[已知问题](KNOWN-ISSUES.md)。游戏内的视觉舒适度因人而异，软件检查不构成缓解效果证明。

## English

First public preview, released on 2026-11-08.

- Regional, Screen-edge and fixed 4x4 Static dots, with separate appearance settings, staggered layouts, a clear center and background brightness/outline adaptation.
- External window capture, grayscale optical flow and keyboard/mouse/controller input prediction drive dynamic dots.
- Central reticle, Chinese/English interface, light/dark themes, visual diagnostics and optional logging.
- A single EXE embeds non-system dependencies, the guide, project license, credits and third-party notices.
- Custom source-available and commercial integration terms are provided in LICENSE.txt.

Release checks: a full build with UCRT64 GCC15.2 and static OpenCV4.12; 17/17 CTest checks passed. Version resources, embedded licenses, archive consistency and desktop guide display were checked.

Exclusive fullscreen, low texture and weapon occlusion can affect behavior. Multi-monitor/high-DPI support and keyboard navigation are incomplete; see [known issues](en/KNOWN-ISSUES.md). Software checks do not establish comfort efficacy.

## 校验 / Checksum

EXE SHA256: `CF3E23678E3E2B3428E1B2D9DB2506557ED46BBD6AB876AF663C7B360A5677E6`.

本页更新不修改已发布的二进制或归档文件。下载包内的文档为发布时的版本；当前说明以仓库文档和对应Release为准。

Documentation updates do not change the published executable or archives. Bundled documents reflect their release-time versions; current notes are available in the repository and the corresponding Release.
