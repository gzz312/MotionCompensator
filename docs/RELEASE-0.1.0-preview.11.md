# 0.1.0-preview.11 — 运动识别与采集更新 / Motion and capture update

2026-11-11。Windows x64预览版。

## 中文

本版更新局部运动识别与采集调度，主要面向第一人称游戏。三种点阵、准星、主题和现有外观设置保持兼容。

### 更新内容

- 采用960×540灰度稠密光流，将81个分析格的可靠观测分别汇总到九个显示区域，保留局部方向与速度差异。
- 动态采集持续更新画面，分析以30次/秒为目标。每次使用最新相邻的两张已回读图像，按真实捕获时间戳换算速度。
- 当源画面实际以60帧/秒交付时，帧对通常约相隔16.7毫秒。实际间隔受窗口更新、系统捕获及GPU回读影响，不等于游戏引擎FPS。
- 使用固定三个GPU回读缓冲及一个可替换分析任务，繁忙时更新到最新画面，避免不断积压旧任务。
- 短暂缺测可延续最近可靠运动，反向与确认停止及时纠正。移动输入只提供短暂起步预测，不会因持续按键而无限维持未知画面的运动。
- 区分可靠静止与观测不足。缺测区域不通过其他区域的向量充当新观测；静态点阵仍由用户手动选择。
- 系统支持并允许时隐藏本会话的采集边框；不支持或未获准时仍可采集，不修改系统设置。
- 移除滚轮输入采集，修正三种外观预览的缩放、裁剪及目标窗口比例同步。
- 调试面板以D标记直接观测格数，日志增加帧对间隔、回读数量、无效帧对、任务替换及处理耗时。日志默认关闭，不自动上传。

### 使用与限制

完整解压Windows ZIP后运行MotionCompensator.exe，也可下载单个EXE。无需安装OpenCV、Python或编译器；运行前请退出其他版本，避免快捷键冲突。

第一人称游戏是当前主要使用场景。第三人称驾驶、大面积静止前景、单调背景或强运动模糊仍可能造成漏检、估速偏大和点阵不连续。更短帧间隔不能恢复模糊中消失的细节，也可能放大微小匹配误差。屏边点阵使用对应区域的观测；只在下方识别到运动时，左右屏边不一定滚动。

分析目标不代表实际吞吐；采集频率、分析频率、渲染提交频率与游戏FPS不同。此版本没有完整的CG自动隐藏功能，也不保证所有游戏兼容或缓解晕动不适。

### 验证范围

21项基础检查通过，覆盖相邻帧与30Hz分析分离、重复时间戳、不可变帧对、重置/停止、速度单位、静止与缺测处理，以及预览和设置。三种点阵的软件自身窗口采集与退出检查通过。

软件自采集的两次动态检查中，帧对间隔中位数约33.3ms、单次分析中位数11.6–11.9ms。该窗口未稳定交付60Hz图像，因此这些结果不证明游戏内16.7ms帧对、端到端延迟降低或游戏帧率改善。不同游戏中的实际体验需分别评价。

## English

This preview updates local motion estimation and capture scheduling, with first-person games as the primary use case. Existing dot styles, reticles, themes and appearance settings remain compatible.

### Changes

- Uses 960×540 grayscale dense optical flow, aggregating reliable observations from 81 analysis cells into nine display regions while preserving local motion differences.
- Capture continuously updates source images; analysis targets 30 updates/s using the latest adjacent readback images and actual source timestamps.
- With a delivered 60Hz source, pair spacing is typically around 16.7ms. Window updates, capture and GPU readback affect actual spacing; this is not game-engine FPS.
- Three fixed GPU staging buffers and one replaceable pending analysis task keep work bounded and favor recent images.
- Recent reliable motion can continue briefly across missing observations. Reversals and confirmed stops correct the output promptly. Movement input supplies only a short initial prediction.
- Reliable stillness remains distinct from missing observations. Missing regions do not receive other regions' vectors as new measurements. Static dots remain manually selected.
- Requests permission to hide the current capture border where supported. Capture continues if unsupported or denied; system settings are not changed.
- Excludes mouse-wheel input and fixes preview scaling, clipping and target-window aspect synchronization.
- D in diagnostics counts directly observed cells. Opt-in logs report pair intervals, readback counts, invalid pairs, replacements and processing time; no automatic upload is performed.

### Usage and limitations

Extract the Windows ZIP and run MotionCompensator.exe, or download the standalone EXE. No OpenCV, Python or compiler installation is needed. Close other versions first to avoid shortcut conflicts.

Third-person driving, large stationary foregrounds, uniform backgrounds and strong motion blur can still cause missed motion, excessive estimates or interrupted dots. Shorter pairs cannot restore blurred detail and may amplify small matching errors. Screen-edge dots use their corresponding regions; left/right edges may stay still when only the lower image has reliable motion.

Target analysis rate is not guaranteed throughput. Capture, analysis, render submission and game FPS are different metrics. Automatic cutscene hiding is not included. Universal compatibility and relief from motion discomfort are not established.

### Validation

All 21 base checks passed, covering adjacent-pair/dispatch separation, timestamps, immutable inputs, reset/shutdown, velocity units, motion/stillness/missing-data policies, previews and settings. All three styles passed desktop self-capture and exit checks.

In two dynamic self-capture checks, median pair spacing was about 33.3ms and processing 11.6–11.9ms. That window did not deliver stable 60Hz images; these checks do not establish 16.7ms gameplay pairing, reduced display latency or improved game FPS. Results vary by game and require separate evaluation.

## 校验 / Checksum

EXE SHA256: `A2C5213C079CE26D2DADD4C4938CD2604804871A755729B8BBBD4B9A5A119142`.
