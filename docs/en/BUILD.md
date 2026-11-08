# Building and local packaging

[中文](../BUILD.md) | **English**

Validated: Windows x64, MSYS2 UCRT64 GCC 15.2, CMake 3.24 or newer, Ninja, static OpenCV 4.12 core/imgproc/video. The MSVC branch is not validated. Non-Windows configuration fails explicitly.

Put the UCRT64 compiler, CMake and Ninja on PATH. Scripts do not install or download tools. Alternatively pass `-Compiler` with the full g++.exe path. An optional `-ToolsRoot` accepts `cmake/data/bin/cmake.exe`, its neighboring ctest.exe, and `bin/ninja.exe`.

Get [OpenCV 4.12.0 source](https://github.com/opencv/opencv/releases/tag/4.12.0), then build matching static dependencies:

```powershell
./build-opencv.ps1 -Source '<OpenCV source directory>'
```

Optional parameters include `-CompilerRoot '<UCRT64/bin>'` and `-ToolsRoot`. Installation defaults to ignored `third_party/opencv/`. For an existing matching installation, pass its OpenCVConfig.cmake directory to `-OpenCVRoot`. Do not mix MinGW libraries with MSVC.

```powershell
./build.ps1 -OpenCVRoot '<static OpenCV installation>'
./package.ps1 -OpenCVRoot '<static OpenCV installation>'
```

The default build directory is `build-preview/`; override it with `-BuildDirectory`. The build script runs CTest. The packager creates `versions/<VERSION>/` and `dist/MotionCompensator-<VERSION>-win-x64.zip` containing the EXE, start guide, known issues, full project license, credits, third-party notices and EXE hash. Only the EXE is required to run.

Standard CMake also works:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DOpenCV_DIR='<static OpenCV installation>'
cmake --build build
ctest --test-dir build --output-on-failure
```

VERSION generates the title, executable resource and manifest versions. `export-source.ps1` exports committed source only; it does not upload anything or include history, logs, cached libraries or previous binaries.

`build.ps1 -SmokeTest` runs a short self-capture check, not a game test. A real interactive desktop may be required; see [testing](TESTING.md).
