# 0.1.0-preview.1 预览验证与发布记录

日期：2026-11-08。候选验证及GitHub发布准备记录。仓库：https://github.com/gzz312/MotionCompensator。二进制发布状态以Releases页面为准。

## 本轮变更

- 对外预览编号统一为0.1.0-preview.1。此前0.14至0.17系列属于内部开发历史，原始文件及本地Git记录保留；不代表功能回退。
- 项目采用《MotionCompensator 源码公开与商业集成许可 1.0》，自定义标识为LicenseRef-MotionCompensator-Integration-1.0。允许免费使用、学习、修改与分享，允许注明出处后的商业集成；禁止独立售卖、换皮售卖、简单包装/捆绑售卖，以及单独收费解锁基于本项目的功能。
- 不为作者设置独立售卖的特殊例外。商业作品的其他独立部分可以闭源；第三方组件保留自身许可。
- 中文正文为准，英文为说明性摘要。完整LICENSE.txt内置EXE并随Windows包和源码提供，使用说明及README同步摘要。
- 清单版本也从VERSION自动生成，避免后续出现标题、EXE版本和清单编号不一致。

## 本轮验证

| 检查 | 结果 |
| --- | --- |
| 新构建目录完整编译 | UCRT64 GCC15.2、现有OpenCV4.12静态安装，成功 |
| CTest | 17/17通过，包含完整许可资源存在、许可页加载以及第三方声明保留检查 |
| 完整许可一致性 | 源码LICENSE.txt的完整字节存在于EXE资源中；ZIP内LICENSE.txt与源文件一致 |
| EXE版本 | FileVersion/ProductVersion为0.1.0-preview.1，预发布标志开启；清单版本0.1.0.1 |
| 包内容 | EXE、中英START-HERE、中英KNOWN-ISSUES、RELEASE-PREVIEW、LICENSE、PROJECT-NOTICES、THIRD-PARTY-LICENSES、SHA256SUMS共10个文件，包内EXE与已检查版本逐字节一致 |
| 实际桌面 | 从新版版本目录启动，标题正确；深色使用说明、许可与致谢可显示；完整新许可及第三方声明已加载 |
| 设置与日志 | 沿用用户已有设置，普通启动未创建logs目录；未调整用户视觉或主题设置 |
| 版本管理 | 仅本地Git与源码备份保留；当前源码发布目标为gzz312/MotionCompensator；旧版仍留在本地 |

EXE SHA256：`CF3E23678E3E2B3428E1B2D9DB2506557ED46BBD6AB876AF663C7B360A5677E6`。

本轮没有修改采集、光流或点阵运动算法，没有操作游戏；不将历史版的软件自采集计数当作本版重新测量的结果。尚未做另一台电脑的首次启动、多显示器/高DPI全面回归或真实游戏效果验证。既有边界见KNOWN-ISSUES.md。

## 编号约定

本系列的预览修订使用0.1.0-preview.2、0.1.0-preview.3等编号；确认该系列正式发布时使用0.1.0。后续修复和功能版本再按实际范围递增。不会自动因构建而改变版本。

## 发布位置

运行目录：`versions/0.1.0-preview.1/`。
Windows包：`dist/MotionCompensator-0.1.0-preview.1-win-x64.zip`。
源码备份：`dist/MotionCompensator-0.1.0-preview.1-source.zip`，不含.git历史、日志、构建缓存和旧版产物。
根目录EXE同步为本版，原0.17.0-preview.1版本文件保留在原版本目录。

许可文本是按项目发布意图整理的自定义条款，尚未经独立法律专业审阅；不声称已验证所有司法地区的可执行性。

## GitHub发布准备

- 中文和English README互相切换；六份核心文档提供成对语言入口，软件已有独立语言切换按钮。
- 新增双语贡献说明，维持完整项目及第三方许可。
- 清理七个旧构建临时目录，约640 MiB；保留当前构建、版本包、日志、源码、依赖和历史记录。
- 源码不上传依赖缓存、旧EXE、原始诊断日志或工作区临时文件。
