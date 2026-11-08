param(
    [string]$BuildDirectory='build-preview',
    [string]$Compiler='',
    [string]$ToolsRoot='',
    [string]$OpenCVRoot=''
)
$ErrorActionPreference='Stop'
$version=(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'VERSION') -Raw).Trim()
if($version -notmatch '^\d+\.\d+\.\d+-preview\.\d+$'){throw 'Expected a local preview version in VERSION'}
& (Join-Path $PSScriptRoot 'build.ps1') -BuildDirectory $BuildDirectory -Compiler $Compiler -ToolsRoot $ToolsRoot -OpenCVRoot $OpenCVRoot
$buildDir=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $PSScriptRoot $BuildDirectory}
$destination=Join-Path $PSScriptRoot "versions/$version"
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$release=Join-Path $destination 'MotionCompensator.exe'
Copy-Item -LiteralPath (Join-Path $buildDir 'MotionCompensator.exe') -Destination $release -Force
$files=@{'LICENSE.txt'='LICENSE.txt';'docs/PREVIEW-README.txt'='START-HERE.txt';'docs/PREVIEW-README.en.txt'='START-HERE.en.txt';'docs/en/KNOWN-ISSUES.md'='en/KNOWN-ISSUES.md';'docs/RELEASE-PREVIEW.md'='RELEASE-PREVIEW.md';'docs/KNOWN-ISSUES.md'='KNOWN-ISSUES.md';'packaging/project-notices.txt'='PROJECT-NOTICES.txt';'packaging/third-party-licenses.txt'='THIRD-PARTY-LICENSES.txt'}
foreach($source in $files.Keys){$target=Join-Path $destination $files[$source];New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null;Copy-Item -LiteralPath (Join-Path $PSScriptRoot $source) -Destination $target -Force}
$hash=(Get-FileHash -LiteralPath $release -Algorithm SHA256).Hash
Set-Content -LiteralPath (Join-Path $destination 'SHA256SUMS.txt') -Value "$hash  MotionCompensator.exe" -Encoding utf8
$dist=Join-Path $PSScriptRoot 'dist';New-Item -ItemType Directory -Force -Path $dist | Out-Null
$zip=Join-Path $dist "MotionCompensator-$version-win-x64.zip"
Compress-Archive -Path (Join-Path $destination '*') -DestinationPath $zip -Force
Write-Host "Local preview: $release"
Write-Host "Archive: $zip"
Write-Host "SHA256: $hash"
