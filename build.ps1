param(
    [string]$BuildDirectory='build-preview',
    [string]$Compiler='',
    [string]$ToolsRoot='',
    [string]$OpenCVRoot='',
    [switch]$SmokeTest
)
$ErrorActionPreference='Stop'
function Find-BuildTool([string]$Name,[string]$Relative) {
    if ($ToolsRoot) { $candidate=Join-Path $ToolsRoot $Relative; if(Test-Path -LiteralPath $candidate){return (Resolve-Path -LiteralPath $candidate).Path} }
    $command=Get-Command $Name -ErrorAction SilentlyContinue
    if($command){return $command.Source}
    throw "Cannot find $Name. Put it on PATH or pass -ToolsRoot. See docs/BUILD.md."
}
if(!$Compiler){$command=Get-Command g++ -ErrorAction SilentlyContinue;if(!$command){throw 'Cannot find g++. Put the UCRT64 compiler on PATH or pass -Compiler.'};$Compiler=$command.Source}
if(!(Test-Path -LiteralPath $Compiler)){throw "Compiler does not exist: $Compiler"}
$Compiler=(Resolve-Path -LiteralPath $Compiler).Path
$env:PATH=(Split-Path $Compiler -Parent)+';'+$env:PATH
$cmake=Find-BuildTool 'cmake' 'cmake/data/bin/cmake.exe'
$ninja=Find-BuildTool 'ninja' 'bin/ninja.exe'
$ctest=Join-Path (Split-Path $cmake -Parent) 'ctest.exe'
$buildDir=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $PSScriptRoot $BuildDirectory}
$options=@('-S',$PSScriptRoot,'-B',$buildDir,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',"-DCMAKE_CXX_COMPILER=$Compiler","-DCMAKE_MAKE_PROGRAM=$ninja")
if($OpenCVRoot){$opencv=(Resolve-Path -LiteralPath $OpenCVRoot).Path;$options+="-DOpenCV_DIR=$opencv"}
& $cmake @options
if($LASTEXITCODE -ne 0){throw 'CMake configuration failed'}
& $cmake --build $buildDir
if($LASTEXITCODE -ne 0){throw 'Build failed'}
& $ctest --test-dir $buildDir --output-on-failure
if($LASTEXITCODE -ne 0){throw 'Checks failed'}
if($SmokeTest){
    $process=Start-Process -FilePath (Join-Path $buildDir 'MotionCompensator.exe') -ArgumentList '--smoke-test' -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(20000)){throw 'Self-capture check did not finish; inspect the test process'}
    if($process.ExitCode -ne 0){throw "Self-capture check failed ($($process.ExitCode)); try the desktop check in docs/TESTING.md"}
}
Write-Host "Ready: $buildDir/MotionCompensator.exe"
