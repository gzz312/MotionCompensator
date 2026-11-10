# 第三方组件与许可

**中文** | [English](en/THIRD-PARTY.md)

项目自身采用根目录LICENSE.txt中的自定义源码公开与商业集成许可。第三方组件保留各自版权与许可，不适用本项目的自定义条款。

| 组件 | 当前用途 | 随包声明 |
| --- | --- | --- |
| OpenCV4.12 core/imgproc/video | 灰度运动估计 | Apache2.0及依赖声明 |
| zlib | OpenCV静态依赖 | zlib许可 |
| libgcc / libstdc++ | GCC静态运行库 | GPLv3及GCC Runtime Library Exception3.1 |
| MinGW-w64 winpthreads | 静态线程支持 | 组件自带许可/版权声明 |

GCC运行库按其GPLv3及Runtime Library Exception条款分发，本项目使用未修改的运行库。具体条件见随附完整许可文本。

完整文本在`packaging/third-party-licenses.txt`，项目状态/致谢在`packaging/project-notices.txt`，项目完整LICENSE.txt也随包提供并内置EXE。许可文件中也保留了OpenCV安装附带的JPEG/PNG/TIFF等声明；不表示这些已关闭的编解码功能在程序中启用。

来源：OpenCV源码/安装中的许可文件，以及构建所用MSYS2 UCRT64运行库包随附声明。
