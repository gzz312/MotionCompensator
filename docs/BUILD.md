# 构建与本地打包

**中文** | [English](en/BUILD.md)

## 已验证环境

Windows x64，MSYS2 UCRT64 GCC15.2，OpenCV4.12静态core/imgproc/video，CMake≥3.24，Ninja。Windows SDK/MinGW头文件需包含Windows Graphics Capture及DirectComposition相关接口。MSVC构建尚未验证。

## 工具准备

安装或准备MSYS2 UCRT64工具链、CMake和Ninja，把其可执行文件目录加入PATH。`build.ps1`从PATH查找工具；`-Compiler`可指定g++.exe完整路径。脚本不下载或安装软件。

若使用本地工具集合，`-ToolsRoot`接受这样的目录布局：`cmake/data/bin/cmake.exe`、`cmake/data/bin/ctest.exe`和`bin/ninja.exe`。一般用户直接使用PATH即可。

## OpenCV

从[OpenCV官方4.12.0源码](https://github.com/opencv/opencv/releases/tag/4.12.0)取得源码后，用与应用相同的UCRT64工具链运行：

```powershell
./build-opencv.ps1 -Source '<opencv源码目录>'
```

也可传`-CompilerRoot '<UCRT64/bin目录>'`及`-ToolsRoot`。该脚本只构建core/imgproc/video及所需依赖，并安装到`third_party/opencv/`。缓存被Git忽略；将源码复制到其他电脑后需要自行重建。不要混用MinGW静态库与MSVC编译器。

已有匹配的静态安装时，直接把包含`OpenCVConfig.cmake`的目录传给`-OpenCVRoot`。不要提交整套头文件和静态库。

## 编译与检查

```powershell
./build.ps1 -OpenCVRoot '<OpenCV静态安装目录>'
```

默认输出`build-preview/`，并运行全部CTest检查。可用`-BuildDirectory`选择其他目录。若使用项目内`third_party/opencv/`缓存，可省略`-OpenCVRoot`。

标准CMake方式：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DOpenCV_DIR='<OpenCV静态安装目录>'
cmake --build build
ctest --test-dir build --output-on-failure
```

CMake在非Windows平台会明确失败。版本由`VERSION`读取并生成程序版本标题，不需要在多个源码位置重复改标题。

## 本地预览包

```powershell
./package.ps1 -OpenCVRoot '<OpenCV静态安装目录>'
```

先构建并检查，再生成`versions/<VERSION>/`和`dist/MotionCompensator-<VERSION>-win-x64.zip`。包内包括单EXE、简短开始说明、已知问题、完整LICENSE.txt、项目致谢、第三方许可和EXE校验值。程序本身仍只需EXE，其余文件便于审阅。

`-SmokeTest`仅是软件自身窗口的WGC自采集检查，约6秒，默认不记录日志。非交互桌面或受限环境可能无法取得WGC权限；不要以管理员权限掩盖该失败。真实桌面的三方案检查程序为构建目录的`desktop_smoke.exe`，显式启用诊断日志，详情见TESTING.md。

源码导出使用`export-source.ps1`，仅包含本地Git当前跟踪文件，不含.git历史、日志、机器依赖缓存或旧版本。该操作生成源码ZIP文件，不包含远程发布步骤。
