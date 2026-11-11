# MotionCompensator

**中文** | [English](README.en.md)

MotionCompensator 是一款主要面向第一人称游戏、旨在缓解 3D 眩晕与晕动不适的 Windows 视觉辅助工具，通过点阵参照与可选的中央准星，帮助改善游玩时的视觉体验。提供分区滚动、屏边滚动和静态三种方案，可分别调整外观，支持背景明暗适配、中英界面及明暗主题。

**Windows x64 · 0.1.0-preview.11 预览版**

[下载预览版](https://github.com/gzz312/MotionCompensator/releases/download/v0.1.0-preview.11/MotionCompensator-0.1.0-preview.11-win-x64.zip) · [发布说明与校验文件](https://github.com/gzz312/MotionCompensator/releases/tag/v0.1.0-preview.11)

## 三种点阵方案

- **分区点阵**：根据画面运动与输入预测，让不同区域的点阵持续滚动。支持交错排布、边界淡化和中央留空。
- **屏边点阵**：在选定屏幕边缘显示较大的圆点，通过滚动、渐隐和大小差异呈现运动。
- **静态点阵**：固定4×4参考点，左右外列带柔边外圈。可通过界面或快捷键选择。

点大小、颜色、透明度和布局可调，三种方案分别保存设置。动态方案还可调整运动强度及反转方向；中央准星通过横排图标按钮切换，支持十字、菱十字、圆点、圆环、留空十字、T形和菱形。

## 实机演示（0.1.0-preview.1）

《收获日2》中的三种点阵效果，点击图片可查看大图。颜色、点大小和透明度均可按喜好调整。

| 分区点阵 | 屏边点阵 | 静态点阵（4×4） |
| --- | --- | --- |
| ![分区点阵：外围细点与中央留空](assets/demo/payday2-regional-user.jpg) | ![屏边点阵：左右屏边的大点阵](assets/demo/payday2-edge-user.jpg) | ![静态点阵：4×4固定参考点](assets/demo/payday2-static-user.jpg) |

**分区点阵动态演示（25帧/秒，约3秒）：**

![收获日2分区点阵动态演示](assets/demo/payday2-regional.gif)

GIF经过缩放和压缩，用于展示点阵外观与运动方式。详细拍摄参数见[素材说明](assets/demo/README.md)。

## 开始使用

1. 下载并解压预览包，运行 `MotionCompensator.exe`。无需安装 OpenCV 或编译器。
2. 启动游戏，优先使用无边框或窗口模式。保持“自动识别前台游戏”开启，切回游戏即可自动连接已识别的游戏；也可刷新窗口列表，手动选择游戏窗口并连接。
3. 选择点阵方案，调整外观。切回游戏查看浮层，切出游戏时浮层隐藏。
4. 按需启用中央准星。界面顶部的“中文 / English”按钮可切换语言，主题按钮可切换浅色、深色或跟随系统。

| 快捷键 | 功能 |
| --- | --- |
| Ctrl+Alt+F8 | 切换点阵方案 |
| Ctrl+Alt+F9 | 显示或隐藏浮层 |
| Ctrl+Alt+F10 | 停止采集和自动连接 |

软件可直接使用单个 EXE，说明与许可文本已内置。运行前请退出其他版本，避免快捷键冲突。

## 使用提示与兼容性

点阵是可选的视觉辅助，体验因人和游戏而异，不能保证消除眩晕。建议先短时体验，出现不适时停止使用。

第三人称驾驶、大片静止前景或单调背景下，可能出现运动漏检、速度偏大或点阵不连续；这些场景尚未专门优化。

独占全屏可能遮挡浮层；强运动模糊、切镜及武器/HUD遮挡可能影响运动估计。多显示器、高DPI和完整键盘导航仍有待完善，详细限制见[已知问题](docs/KNOWN-ISSUES.md)。

程序通过外部 Windows Graphics Capture 采集窗口，结合局部灰度稠密光流及键鼠/手柄输入预测驱动点阵。采集持续更新相邻画面，分析以30次/秒为目标，实际频率受画面交付与处理能力影响。它不使用 Hook、DLL 注入、游戏内存读取或反作弊绕过；使用前仍应确认目标游戏允许外部浮层。

## 问题反馈

遇到问题可提交 [Issue](https://github.com/gzz312/MotionCompensator/issues)，请附上软件版本、游戏和窗口模式、显示缩放及复现步骤。

右下角“日志”默认关闭。需要诊断时勾选，复现后取消，文件保存在 EXE 旁的 `logs/` 中，不会自动上传。分享前请检查其中的进程信息等本地内容。[反馈与贡献指南](CONTRIBUTING.md) · [本地数据与隐私](docs/PRIVACY.md)

## 使用许可

本项目采用[源码公开与商业集成许可](LICENSE.txt)：允许免费使用、学习、修改和分享，也允许注明出处后集成到具有独立实质功能的商业作品中；禁止独立售卖、换皮售卖及单独收费解锁本项目功能。

分发时须保留完整许可并注明 MotionCompensator、作者 gzz312 和来源。完整规则以许可中文正文为准，第三方组件遵守各自许可证。[第三方许可说明](docs/THIRD-PARTY.md)

## 从源码构建

已验证环境为 Windows x64、MSYS2 UCRT64 GCC15.2、CMake、Ninja 和匹配的 OpenCV4.12 静态库；MSVC 构建尚未验证。

```powershell
./build.ps1 -OpenCVRoot '<OpenCV静态安装目录>'
./package.ps1 -OpenCVRoot '<OpenCV静态安装目录>'
```

依赖准备及完整步骤见[构建说明](docs/BUILD.md)。[测试说明](docs/TESTING.md) · [版本记录](docs/HISTORY.md) · [架构说明](docs/DEVELOPMENT.md)

## 参考与致谢

屏边点阵的视觉方案参考了 shsr07（拾荒散人）的 [MotionStabilizer](https://github.com/shsr07/MotionStabilizer)，感谢作者分享设计思路与创意。[作者B站视频](https://www.bilibili.com/video/BV1saKg6XEvg/)

交错排布与周期连续性思路参考了 Miguel Peixoto 的 MotionCues，视觉锚点设计参考了 Peregrine。完整来源与代码比对记录见[设计参考与致谢](docs/CREDITS.md)。
