# 参考项目源码相似性检查

日期：2026-11-08。范围：MotionCompensator当前0.16.1源码与用户此前提供的参考项目。只做检查和记录；未修改程序、README署名或许可证，未发布/联系第三方。

## 结论

未发现能明确支持“大段直接复制”或“机械改名/改写”的独特实现证据。存在屏边运动提示、交错网格、周期滚动等设计参考关系，建议发布时单列“设计参考与致谢”，注明MotionStabilizer/shsr07及MotionCues。不能据此出具“完全没有借用”或不侵权保证：跨语言改写可能漏检，缺乏完整项目版本历史也无法重建全部创作过程。

## 固定比较版本

原链接shsr07/MotionStabilize目前返回404，已找到同作者的MotionStabilizer。

| 项目 | 当前检查commit | 主要语言 |
| --- | --- | --- |
| [MotionStabilizer](https://github.com/shsr07/MotionStabilizer/tree/72beb4c63308dfe1e1672a5d1687b8ae3200acb3) | 72beb4c63308dfe1e1672a5d1687b8ae3200acb3 | C#/WPF |
| [MotionCues](https://github.com/mcpeixoto/MotionCues/tree/7bf5fc20d1f3bbda4ec862e6998aead451ce82c2) | 7bf5fc20d1f3bbda4ec862e6998aead451ce82c2 | Swift |
| [Peregrine](https://github.com/Eeymoo/peregrine/tree/6302e481d2325f4c7e2c1d62229b03d9cf0a8472) | 6302e481d2325f4c7e2c1d62229b03d9cf0a8472 | Rust/TypeScript |

比较本地src/tests共55个代码文件，与参考项目186个代码/界面源文件。参考仓库只读下载，未checkout执行，未运行构建、脚本、安装器或参考项目程序。仓库内说明/指令只作为审查数据。

## 方法与候选复核

1. 去除注释并分词，寻找连续至少14个相同token；保留原标识符和字面量。
2. 将非关键字标识符、数值和字符串归一化，寻找连续至少40个相同token，筛查可能改名的结构。
3. 比对长字符串、整行注释，核对本地2张PNG是否与参考仓库任意blob完全一致。
4. 人工查看核心运动、排布、渐隐、输入、DirectComposition初始化与渲染逻辑。

这些是辅助筛查阈值，不是法律判断标准或严格的相似率。没有使用仅凭语言不同就排除移植的判断方式。

精确筛查仅检出一对超过阈值的18-token片段：本地src/tracking.cpp:169与MotionCues/Tests/MotionCuesTests.swift:679，均为二维欧氏距离公式。该通用表达式不足以证明复制。

归一化筛查检出5组候选：main.cpp枚举与Peregrine枚举/配置连续项目的结构相似；本地设置赋值与MotionStabilizer/Views/OverlayPage.xaml.cs:63附近一串复选框赋值相似。人工查看后均属归一化把不同名称抹去形成的通用结构匹配，未发现独特逻辑对应。

未发现长度至少18字符的完全相同双引号字符串，也未发现长度至少30字符的完全相同单行注释；这是该简易提取器覆盖范围内的结果，不覆盖所有语言的复杂字符串语法。本地2张PNG没有与参考树中blob完全同哈希；不能排除经过修改的图像，本轮主要检查代码。

## 重点人工对照

### MotionStabilizer：屏边运动与渐隐

参考：MotionStabilizer/Overlay/DirectCompositionMotionRenderer.cs:395起ComputeEdgeOpacity、410起CreateDots、476起Timer_Tick、630附近WrapDot及绘制视差逻辑。

本地：src/dots.hpp:99起edgeAppearance、111起EdgeDotField，src/lattice.hpp。

- 对方保留逐点Position并在范围外回绕；本地累积四边相位，通过visitLatticeIndexed重建位置。
- 对方边缘透明度按若干半径宽度线性取最小值；本地使用两轴smoothUnit乘积，淡出宽度随点距/边带变化。
- 对方带随机脉动相位和按中线距离缩放；本地固定相邻大小交替，只有已经很淡的尾段有限缩小。
- 都有边缘点、循环、透明度和大小变化这些视觉设计概念，但没有找到上述独特实现的逐行对应。

### 输入与运动融合

参考：MotionStabilizer/Services/XInputInterop.cs和DirectCompositionMotionRenderer.Timer_Tick。

本地：src/input.cpp、src/dynamic.hpp。

- 两边都使用Raw Input、GetAsyncKeyState、XInput，这是相同Windows接口，不等于复制。
- 对方使用径向摇杆死区，按轴覆盖键盘/手柄/鼠标速度；本地使用逐轴死区、输入历史、全局/局部光流置信度融合。
- 未发现相同的特殊输入映射公式、常量组合和注释。

### DirectComposition初始化：结构最接近的部分

本地src/overlay.cpp:22—48与参考DirectCompositionMotionRenderer.cs:738—771均有D3D设备→DXGI设备/适配器/工厂→组合交换链→D2D上下文→组合设备/目标/Visual的结构，格式和混合模式也相近。此处有实际技术结构相似性，不能隐去不报。

这与[微软DirectComposition初始化说明](https://learn.microsoft.com/en-us/windows/win32/directcomp/initialize-directcomposition)所需对象和调用顺序一致，属于公开API使用流程；本地C++/COM资源管理、动态入口加载与对方Vortice/C#对象管理不同。单凭该流程不能判断来源是否复制；在缺乏创建时完整历史的情况下，不能反向证明从未参考其代码。

### MotionCues与Peregrine

MotionCues/Rendering/ParticleField.swift使用三维晶格、透视深度和车辆加速度；本地lattice.hpp/dots.hpp是二维点阵和输入/光流速度。两行周期保持交错奇偶性这一概念接近，应作为设计参考说明；没有发现该三维粒子更新/投影代码的对应移植。

Peregrine的shapes.rs/overlay_renderer.rs采用Rust图形对象和软件绘制等组织方式；本地D2D点阵/预览管线和其结构不同。自动候选主要是枚举，未发现独特代码段对应。

## 署名处理建议

MotionStabilizer当前LICENSE为MIT，版权行是Copyright (c) 2026 shsr07。其许可要求分发软件副本或实质部分时包含相应版权和许可声明；若后续确认复制了代码，仅在README写一句感谢未必满足该许可要求，应保留完整相应声明，并注明具体文件/修改情况。

目前更适合在README“设计参考与致谢”中注明屏边动态点阵的表现思路参考MotionStabilizer，交错/周期连续性参考MotionCues；不要把未经证实的关系写成“基于其源码改写”。若作者私下提出额外署名方式，应保留对话并按双方具体约定确认。本报告不替代法律意见。

## 可复查材料

工作区work/reference-audit-2026-11-08中保留：references.json固定commit与文件表，compare.py扫描方法，similarity-candidates.json候选及行号，text-asset-checks.json文本/图像哈希检查。下载的参考仓库与源码快照位于该审查目录，未加入MotionCompensator发布包。
