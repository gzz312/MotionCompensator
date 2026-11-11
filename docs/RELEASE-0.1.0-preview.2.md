# 0.1.0-preview.2 — 预览更新 / Preview update

2026-11-10。新增准星样式与图标切换，整理中英文使用说明。此版本仍为预览版。

## 中文

- 准星下拉菜单改为横排图标按钮：关闭、十字、菱十字、圆点、圆环、留空十字、T形、菱形。
- 当前选项以蓝色标记，悬停可查看名称与操作提示。颜色、透明度及大小仍由共用控件调整，设置自动保存。
- 原有关闭、十字与菱十字配置继续有效；切换点阵方案保留准星选择。
- 图标、外观预览及游戏浮层使用同一套几何图形定义。准星固定在屏幕中心，不跟随后坐力或弹着位置。
- 重新整理中英文使用说明、致谢与随包入门说明，按用途、操作、准星、诊断和常见问题组织内容。
- 修复切换语言时复选框绕过主题绘制而出现黑字、白框的问题。
- 窗口标题、EXE资源和清单的版本修订号统一来自 VERSION。

兼容性：保留原有准星设置。新版准星的游戏内显示尚待进一步验证。

采集、光流与点阵运动算法未调整。游戏中的视觉舒适性由实际体验另行评价，软件自身检查不能证明缓解疗效。

## English

This preview adds reticle styles and icon selection, with revised bilingual instructions. It remains a prerelease.

- Eight compact icon buttons replace the reticle dropdown: Off, Cross, Diamond cross, Dot, Ring, Open cross, T-shape and Diamond.
- Blue marks the selected style; hover for its name and help. Existing color, transparency and size controls remain shared and saved automatically.
- Existing Off/Cross/Diamond-cross preferences keep their original IDs. Changing dot styles preserves the reticle.
- Icons, the preview and the overlay share the same geometry. Reticles are fixed center references, not recoil or impact indicators.
- The bilingual guide explains reticle selection, size, diagnostics and common problems, with quick-start instructions included in the download.
- Checkbox translation updates use themed painting to avoid dark-theme black text and white boxes.
- The application title, EXE resource and manifest revision derive from VERSION.

Compatibility: existing reticle settings are retained. Further in-game validation of the new shapes is pending.

Capture, optical flow and dot-motion algorithms are unchanged. Software checks do not establish motion-discomfort relief.

## 软件自身验证 / Software validation

- 新建构建目录，以 UCRT64 GCC 15.2 / OpenCV 4.12 静态库编译成功。
- CTest 17/17 通过。扩展检查覆盖八种准星的保存/重载、4/16/64尺寸的实际预览像素、圆形透明度、T形上臂缺省、浅深主题按钮及复选框翻译重绘。
- 实际桌面检查：图标横排完整可见；选择圆环后预览同步；重启保留圆环；中英文字切换；跟随系统/浅色/深色循环；使用说明加载了新版内容。
- FileVersion/ProductVersion 为 0.1.0-preview.2，Windows数值版本为 0.1.0.2。
- 新版准星的游戏内显示、端到端延迟与舒适度效果尚未验证。

Built with UCRT64 GCC 15.2 and static OpenCV 4.12. All 17 CTest checks passed. Desktop checks covered icon selection and preview updates, persistence after restart, bilingual labels, theme cycling and guide loading. These checks do not establish in-game comfort or performance benefits.

EXE SHA256: `D186F63FB3C427AC9965C61275C6E6308D1DCD82866428E99930661128598C4F`.
