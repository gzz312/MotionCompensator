# 0.1.0-preview.14 — 设置界面与滑块响应 / Settings and slider responsiveness

2026-11-12 · Windows x64预览版 / Windows x64 preview

## 中文

本版更新设置界面，并修复快速拖动滑块时响应滞后的问题。原有采集、运动识别和点阵运动方式保持不变。

### 更新内容

- 横向工作台：顶部连接游戏，左侧显示更大的外观预览，右侧通过“点阵 / 准星”切换设置。
- 一级页面导航使用文字与短指示条，三种点阵方案使用分段选择，区分导航层级。
- 取消“更多设置”折叠，直接按外观和运动分组；静态方案隐藏运动分组。
- 右侧内容超出高度时支持滚轮与拖动滚动条。顶部导航、左侧预览及底部日志/调试操作保持固定。
- 滚轮在设置区内用于滚动，不直接修改滑块；展开的下拉列表保留自身的滚动选择。
- 拖动时优先绘制滑钮，只更新当前参数与数值标签；相同数值不重复发送拖动通知。
- 拖动期间合并外观预览刷新，最高目标为每秒30次；松手、取消或键盘调整结束时请求最终预览。该合并不限制滑钮的位置更新频率。
- 预览渐变改用缓存像素绘制，减少重复绘图对象。常规状态区简化，详细参数可悬停查看或开启调试显示。
- 保留三种点阵独立配置、准星共用配置、明暗主题、中英切换和自动保存，使用说明同步更新。

### 使用与验证范围

下载Windows ZIP完整解压后运行MotionCompensator.exe，或直接下载单个EXE。无需安装Python、OpenCV或编译器；启动前退出旧版，避免快捷键冲突。

21项基础检查与三种点阵的软件自身窗口采集检查通过。实际界面检查覆盖正反向拖动、越界松手、键盘微调、预览同步、方案切换和重启保存。没有测量端到端显示延迟，不能据此承诺所有设备的响应时间或完全无闪烁。

窗口仍按整体比例缩放，小屏幕、高DPI、多显示器与完整键盘导航仍需进一步适配。当前主要面向第一人称游戏；第三人称驾驶、单调背景、静止前景和强运动模糊的运动识别限制保持不变。此预览不保证缓解晕动不适，也没有完整的CG自动隐藏功能。

## English

This preview updates the settings interface and improves responsiveness during rapid slider dragging. Capture, motion estimation and dot-motion algorithms are unchanged.

### Changes

- A wider workbench puts game connection at the top, a larger preview on the left and separate Dots / Reticle settings on the right.
- Text navigation with a short active-page indicator is distinct from the segmented dot-style selector.
- Removes the More settings disclosure and directly groups appearance and motion controls. Static dots hide the motion group.
- Overflowing settings support the wheel and a draggable scrollbar. Page navigation, preview and footer logging/diagnostic actions stay fixed.
- The wheel scrolls settings instead of directly changing sliders. Open dropdown lists retain their own scrolling.
- Dragging prioritizes thumb presentation and updates only the changed parameter and label. Identical values do not emit duplicate tracking notifications.
- Appearance previews are coalesced to a target maximum of 30 updates/s during dragging, with a final preview requested on release, cancellation or completed keyboard adjustment. This does not cap thumb position updates.
- The gradient preview uses cached pixels to reduce drawing-object churn. Normal status is simplified; hover or enable diagnostics for detailed measurements.
- Independent dot profiles, shared reticle settings, themes, Chinese/English UI and automatic saving remain compatible. The guide is updated.

### Use and validation

Extract the Windows ZIP and run MotionCompensator.exe, or download the standalone EXE. Python, OpenCV and compiler installation are not required. Close older versions first to avoid shortcut conflicts.

All 21 base checks and desktop self-capture checks for all three styles passed. Actual-window checks covered forward/reverse dragging, release outside the track, keyboard adjustment, preview synchronization, profile switching and persistence after restart. End-to-end display latency was not measured; these checks do not guarantee response time or absence of flicker on every system.

The window still scales proportionally. Small-screen, high-DPI, multi-monitor and complete keyboard adaptation remain incomplete. First-person games remain the primary focus; third-person driving, uniform backgrounds, stationary foregrounds and strong motion blur remain limitations. This preview does not guarantee relief from motion discomfort and does not include complete automatic cutscene hiding.

## 校验 / Checksum

EXE SHA256: `da7059f16c7ce97e9a5f2783f946ad1bf9a15d83088cb6116a4f4f327335048d`
