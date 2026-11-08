# 本地构建依赖

opencv/是本机OpenCV 4.12.0静态安装（core、imgproc、video），不需要随EXE分发，也被Git忽略。重建步骤见根目录build-opencv.ps1和docs/BUILD.md。

需要保留的依赖许可说明位于packaging/third-party-licenses.txt，并作为RCDATA资源嵌入EXE，可从使用说明末尾查看。这里没有安装/下载新的第三方库。
