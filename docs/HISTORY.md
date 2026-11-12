# 版本记录 / Changelog

## 0.1.0-preview.14 — 设置界面与滑块响应（2026-11-12）

- 设置界面改为横向工作台：顶部连接、左侧预览、右侧点阵与准星独立页面。
- 页面导航与点阵方案分层显示，外观和运动分组直接展开，右侧内容支持滚动。
- 修复快速拖动滑块不跟手的问题，减少重复通知与整套设置处理，合并预览刷新并在松手后同步最终状态。
- 优化预览渐变绘制，简化常规状态显示，更新中英说明。

Introduces a wider workbench with separate Dots and Reticle pages, distinct page/style selection and scrollable appearance/motion groups. Improves slider responsiveness by avoiding duplicate/full-settings work, coalesces previews and synchronizes final values on release. Updates preview drawing, status presentation and the bilingual guide. Capture and dot-motion algorithms are unchanged.

[发布说明 / Release notes](RELEASE-0.1.0-preview.14.md)

## 0.1.0-preview.11 — 运动识别与采集更新（2026-11-11）

- 更新局部稠密光流与相邻采集帧调度，分析以30次/秒为目标。
- 保留有时限的运动延续，区分可靠静止与观测不足，避免持续按键无限驱动缺测区域。
- 支持在系统允许时隐藏采集边框，移除滚轮输入采集，修正外观预览。
- 增加直接观测与帧对诊断，保持现有外观设置兼容。

Updates local dense flow, adjacent-source pairing, bounded continuation and stillness handling. Adds supported capture-border hiding, excludes wheel input and fixes previews. Existing appearance settings remain compatible. First-person games are the primary focus; uniform backgrounds and third-person driving remain limited.

[发布说明 / Release notes](RELEASE-0.1.0-preview.11.md)

## 0.1.0-preview.2 — 准星与界面更新（2026-11-10）

- 新增圆点、圆环、留空十字、T形和菱形准星，连同十字、菱十字及关闭选项，通过横排图标按钮切换。
- 准星颜色、透明度及大小继续可调，已有设置保持兼容，切换点阵方案保留准星选择。
- 修复切换语言后深色主题复选框出现黑字、白框的问题。
- 使用说明补充准星样式、尺寸含义和操作方法。

采集、光流与点阵运动算法保持不变。详见[发布说明](RELEASE-0.1.0-preview.2.md)。

Reticle styles now use compact icon buttons, with additional Dot, Ring, Open cross, T-shape and Diamond options. Existing settings remain compatible. Dark-theme checkboxes render correctly after language changes, and the guide explains reticle selection and size. Capture, optical flow and dot-motion algorithms are unchanged.

## 0.1.0-preview.1 — 首个公开预览版（2026-11-08）

- 提供分区点阵、屏边点阵和4×4静态点阵，支持独立外观设置、交错排布和背景适配。
- 使用外部窗口采集、灰度光流与键鼠/手柄输入预测驱动动态点阵。
- 提供中央准星、中英界面、明暗主题、调试可视化与可选日志。
- 使用说明、项目许可、设计致谢与第三方声明内置于程序，并随分发包提供。

First public preview with Regional, Screen-edge and 4×4 Static dots, independent appearance settings, background adaptation, an optional reticle, Chinese/English UI, themes and opt-in diagnostics. Dynamic dots use external window capture, grayscale optical flow and input prediction. Instructions and license notices are included in the application and release package.

[发布说明 / Release notes](RELEASE-PREVIEW.md) · [下载 / Downloads](https://github.com/gzz312/MotionCompensator/releases)
