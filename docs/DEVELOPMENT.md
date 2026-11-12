# 架构与实现 / Architecture

## 中文

MotionCompensator 使用 Windows 原生 C++20，实现外部窗口采集、运动估计和透明点阵浮层。

- **窗口与采集**：Win32提供窗口选择与前台目标识别，Windows Graphics Capture提供游戏画面。
- **运动估计**：局部灰度稠密光流与只读键鼠/手柄输入观察共同驱动动态点阵。输入代表操作意图，不等同于角色或车辆的实际运动。
- **点阵渲染**：Direct2D与DirectComposition绘制透明、鼠标穿透的浮层，提供分区滚动、屏边滚动与固定4×4静态参照。静态方案由用户选择。
- **外观适配**：点阵颜色、大小、透明度、布局与准星可调；背景亮度采样支持同色相明暗变化及双层描边。
- **诊断与界面**：提供中英界面、明暗主题、统一调试可视化、可选日志、错误状态与悬停提示。

程序不使用Hook、DLL注入、游戏进程内存读取或反作弊绕过。具体兼容性及限制见[已知问题](KNOWN-ISSUES.md)。

软件检查用于验证工程行为，游戏内体验需独立评价，不能将构建或自动检查结果当作晕动缓解效果的证明。[构建说明](BUILD.md) · [测试方法](TESTING.md) · [版本记录](HISTORY.md)

设计来源及比对范围见[致谢](CREDITS.md)和[参考代码比对记录](REFERENCE-CODE-AUDIT-2026-11-08.md)。参考项目作者的署名不表示其参与开发或为本项目背书。

设置工作台由原生Win32控件与自绘界面组成。页面导航、点阵方案和可滚动的外观/运动设置分层显示；滑块优先更新当前参数与标签，预览单独合并刷新。

## English

MotionCompensator is a native Windows C++20 application for external window capture, motion estimation and transparent visual overlays.

- Win32 supplies window selection and foreground-target detection; Windows Graphics Capture supplies images.
- Local grayscale dense optical flow and read-only keyboard/mouse/controller observations drive dynamic dots. Input indicates intent, not actual character or vehicle motion.
- Direct2D and DirectComposition render a transparent, click-through overlay with Regional, Screen-edge and user-selected 4×4 Static styles.
- Adjustable dot/reticle appearance and background brightness sampling support same-hue brightness changes and layered outlines.
- The native Win32 settings workbench separates page navigation, dot-style selection and scrollable appearance/motion controls. Slider input updates only its parameter and label; preview updates are coalesced separately. Chinese/English labels, themes, visual diagnostics, opt-in logs, error states and hover help remain available.

No hooks, DLL injection, game-memory reading or anti-cheat bypasses are used. See [known issues](en/KNOWN-ISSUES.md) for compatibility limits. Software checks validate implementation behavior and do not establish motion-discomfort relief. See the [build guide](en/BUILD.md), [testing guide](en/TESTING.md), [changelog](HISTORY.md) and [credits](en/CREDITS.md).
