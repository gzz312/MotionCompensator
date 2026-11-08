param([string]$Destination='')
$ErrorActionPreference='Stop'
$version=(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'VERSION') -Raw).Trim()
if(!$Destination){$Destination=Join-Path $PSScriptRoot "dist/MotionCompensator-$version-source.zip"}
if(![IO.Path]::IsPathRooted($Destination)){$Destination=Join-Path $PSScriptRoot $Destination}
$parent=Split-Path $Destination -Parent;if($parent){New-Item -ItemType Directory -Force -Path $parent | Out-Null}
& git -C $PSScriptRoot archive --format=zip --output=$Destination HEAD
if($LASTEXITCODE -ne 0){throw 'Source export requires a local Git commit; no upload was performed'}
Write-Host "Local source archive: $Destination"
