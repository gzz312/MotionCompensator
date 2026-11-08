# 贡献与反馈 / Contributing

[中文](#中文) | [English](#english)

## 中文

问题反馈和改进建议都欢迎。提交问题前请查看README与已知问题，描述版本、Windows/游戏版本、窗口模式、分辨率与缩放、输入设备、点阵方案及复现步骤。日志为可选；先检查并去除个人内容，不要上传完整工作目录或游戏画面。

代码修改请说明问题、最终行为和实际验证；动态模式不得自动退回静态。不要引入Hook、DLL注入、游戏内存读取、反作弊绕过或默认日志/上传。保持渲染与采集分析异步，并同步中文和英文文档。

请使用与应用匹配的UCRT64/OpenCV工具链构建并执行CTest；GUI和真实游戏验证分别记录，不把软件检查当作缓解疗效证明。只改文档时检查两种语言的链接和含义一致即可。

提交代码意味着你有权按本项目LICENSE.txt提供该项贡献；此声明不要求版权转让，也不授予维护者额外的独立售卖权。第三方代码必须有兼容许可并保留完整出处，不要复制许可不清晰的实现。

## English

Issue reports and improvement proposals are welcome. Read the README and known issues first. Include the app version, Windows/game version, window mode, display resolution/scaling, input device, dot style and reproduction steps. Logs are optional: remove private content first and do not upload the full workspace or game images.

For code changes, explain the problem, resulting behavior and actual validation. Dynamic modes must not automatically fall back to Static. Do not introduce hooks, DLL injection, game-memory reading, anti-cheat bypasses or default logging/uploading. Keep capture and analysis asynchronous to rendering, and update both documentation languages.

Build with matching UCRT64/OpenCV dependencies and run CTest. Record GUI and game checks separately; software tests do not establish comfort efficacy. Documentation-only changes need link and language-consistency checks.

By submitting code, you represent that you have the right to offer that contribution under this project's LICENSE.txt. This does not transfer copyright or give maintainers extra standalone-sale rights. Third-party code requires compatible terms and preserved provenance; do not copy implementations with unclear licensing.
