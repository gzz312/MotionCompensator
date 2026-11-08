param(
    [Parameter(Mandatory=$true)][string]$Source,
    [string]$CompilerRoot='',
    [string]$ToolsRoot=''
)
$ErrorActionPreference='Stop'
$sourceDir=(Resolve-Path -LiteralPath $Source).Path
if (!(Test-Path -LiteralPath (Join-Path $sourceDir 'modules\video\CMakeLists.txt'))) { throw 'Expected official OpenCV source directory' }
if(!$CompilerRoot){$gcc=Get-Command gcc -ErrorAction SilentlyContinue;if(!$gcc){throw 'Cannot find gcc. Put the UCRT64 toolchain on PATH or pass -CompilerRoot.'};$CompilerRoot=Split-Path $gcc.Source -Parent}
$cmake=if($ToolsRoot){Join-Path $ToolsRoot 'cmake/data/bin/cmake.exe'}else{(Get-Command cmake -ErrorAction Stop).Source}
$ninja=if($ToolsRoot){Join-Path $ToolsRoot 'bin/ninja.exe'}else{(Get-Command ninja -ErrorAction Stop).Source}
$env:PATH=$CompilerRoot+';'+$env:PATH
$buildDir=Join-Path $PSScriptRoot 'build-opencv'
$prefix=Join-Path $PSScriptRoot 'third_party\opencv'
$options=@(
    '-G','Ninja','-DCMAKE_BUILD_TYPE=Release',"-DCMAKE_MAKE_PROGRAM=$ninja",
    "-DCMAKE_C_COMPILER=$CompilerRoot\gcc.exe","-DCMAKE_CXX_COMPILER=$CompilerRoot\g++.exe",
    "-DCMAKE_INSTALL_PREFIX=$prefix",'-DBUILD_LIST=core,imgproc,video',
    '-DBUILD_SHARED_LIBS=OFF','-DBUILD_TESTS=OFF','-DBUILD_PERF_TESTS=OFF',
    '-DBUILD_EXAMPLES=OFF','-DBUILD_opencv_apps=OFF','-DBUILD_JAVA=OFF',
    '-DBUILD_opencv_python2=OFF','-DBUILD_opencv_python3=OFF','-DBUILD_opencv_dnn=OFF',
    '-DBUILD_opencv_gapi=OFF','-DWITH_IPP=OFF','-DWITH_ITT=OFF','-DWITH_OPENCL=OFF',
    '-DWITH_TBB=OFF','-DWITH_FFMPEG=OFF','-DWITH_MSMF=OFF','-DWITH_DSHOW=OFF',
    '-DWITH_TIFF=OFF','-DWITH_JPEG=OFF','-DWITH_PNG=OFF','-DWITH_OPENEXR=OFF',
    '-DWITH_WEBP=OFF','-DWITH_OPENJPEG=OFF','-DWITH_JASPER=OFF','-DWITH_ADE=OFF',
    '-DWITH_PROTOBUF=OFF','-DBUILD_PROTOBUF=OFF','-DCPU_DISPATCH=',
    '-DVIDEOIO_ENABLE_PLUGINS=OFF','-DOPENCV_GENERATE_PKGCONFIG=OFF',
    '-DENABLE_PRECOMPILED_HEADERS=OFF'
)
& $cmake -S $sourceDir -B $buildDir @options
if ($LASTEXITCODE -ne 0) {throw 'OpenCV configuration failed'}
& $cmake --build $buildDir --target install --parallel 8
if ($LASTEXITCODE -ne 0) {throw 'OpenCV build failed'}
Copy-Item -LiteralPath (Join-Path $sourceDir 'LICENSE') -Destination (Join-Path $prefix 'LICENSE')
Write-Host "OpenCV ready: $prefix"
