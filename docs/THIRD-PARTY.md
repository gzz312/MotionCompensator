# 第三方组件与许可

**中文** | [English](en/THIRD-PARTY.md)

项目自身采用根目录LICENSE.txt中的自定义源码公开与商业集成许可，未将MotionCompensator标为MIT/GPL项目。本许可不重新授权或限制第三方组件本身。第三方组件的权利与许可证保留。

| 组件 | 当前用途 | 随包声明 |
| --- | --- | --- |
| OpenCV4.12 core/imgproc/video | 灰度运动估计 | Apache2.0及依赖声明 |
| zlib | OpenCV静态依赖 | zlib许可 |
| libgcc / libstdc++ | GCC静态运行库 | GPLv3及GCC Runtime Library Exception3.1 |
| MinGW-w64 winpthreads | 静态线程支持 | 组件自带许可/版权声明 |

GCC运行库附带的Runtime Library Exception对符合条件的独立模块提供额外链接/分发许可；不能仅凭运行库文本中的GPL字样就认定项目自身已选择GPL。当前采用普通GCC工具链编译，未修改运行库。将来正式分发前仍应核对实际依赖和对应声明。

完整文本在`packaging/third-party-licenses.txt`，项目状态/致谢在`packaging/project-notices.txt`，项目完整LICENSE.txt也随包提供并内置EXE。许可文件中也保留了OpenCV安装附带的JPEG/PNG/TIFF等声明；不表示这些已关闭的编解码功能在程序中启用。

来源：OpenCV源码/安装中的许可文件，以及构建所用MSYS2 UCRT64运行库包随附声明。
