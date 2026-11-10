# Third-party components and licenses

[中文](../THIRD-PARTY.md) | **English**

Project-original material uses the custom source-available and commercial integration terms in [LICENSE.txt](../../LICENSE.txt). These do not replace or restrict third-party components' own licenses.

| Component | Use | Notices |
| --- | --- | --- |
| OpenCV 4.12 core/imgproc/video | Grayscale motion estimation | Apache 2.0 and dependency notices |
| zlib | Static OpenCV dependency | zlib license |
| libgcc / libstdc++ | GCC static runtime | GPLv3 with GCC Runtime Library Exception 3.1 |
| MinGW-w64 winpthreads | Static threading support | Supplied copyright/license terms |

GCC runtime libraries are distributed under their GPLv3 and Runtime Library Exception terms. This project uses unmodified runtime libraries; see the bundled complete texts for the conditions.

Full third-party texts are in [packaging/third-party-licenses.txt](../../packaging/third-party-licenses.txt), credits in [project-notices.txt](../../packaging/project-notices.txt), and the project license in [LICENSE.txt](../../LICENSE.txt). All are embedded in the EXE. The supplied OpenCV notices also retain JPEG/PNG/TIFF notices; this does not mean those disabled codec features are enabled in the app.

Notice sources are the OpenCV source/install distribution and the MSYS2 UCRT64 runtime packages used to build the executable.
