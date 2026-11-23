# MotionCompensator 0.2.0-preview.10

## 中文

这版新增网格点阵、游戏独立配置和菜单按键规则。静态、分区和屏边点阵依然可在「视觉效果」中选择。

- **网格点阵**按圆点所在位置响应画面运动，并根据间距补充或回收节点。可选的「纵深效果」用大小和红蓝配色区分画面扩张与收缩。
- **游戏配置**支持登记窗口、自动连接前台的已登记游戏，以及为单个游戏保存外观和动作按键。
- **菜单规则**可设置开启键、返回键和返回次数。菜单期间可隐藏浮层、暂停动态滚动或临时显示静态点阵。
- **设置和诊断**分为总览、视觉效果、游戏按键配置、运行诊断和通用设置。网格统计显示可见点、补点、回收和更新耗时。中英文使用说明介绍了四种点阵的适用场景。

下载 Windows x64 压缩包，解压后双击 `MotionCompensator.exe`。在「总览」选中游戏窗口并连接。需要更新旧版本时，请先退出正在运行的实例。

预览版适合先在无边框或窗口模式下试用。道路、天空等纹理较少的区域和较大的固定前景可能影响运动估计。详见[已知问题](KNOWN-ISSUES.md)。

## English

This preview adds Mesh dots, per-game profiles and menu key rules. Static, Regional and Screen-edge dots are available alongside Mesh dots in Visual effects.

- **Mesh dots** follow motion at their screen positions and add or retire nodes to maintain spacing. Optional Depth effect uses dot size and red or blue tint to distinguish expansion from contraction.
- **Game profiles** let you register windows, reconnect when a registered game comes to the foreground, and save appearance and movement keys per game.
- **Menu rules** let you record opening and return keys and set a return count. During a menu, you can hide the overlay, pause motion or switch temporarily to Static dots.
- **Settings and diagnostics** have separate pages for Overview, Visual effects, Game keys, Diagnostics and General. Mesh diagnostics report visible dots, node changes and update timings. The bilingual User Guide explains when to try each dot style.

Download and extract the Windows x64 ZIP, then open `MotionCompensator.exe`. Select a game window and connect from Overview. Close any earlier instance before updating.

Try this preview in borderless or windowed mode. Plain surfaces such as roads and skies, or large stationary objects in the foreground, can affect motion estimates. See [Known issues](en/KNOWN-ISSUES.md).
