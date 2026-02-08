@echo off
setlocal

REM Run from the folder where this .bat is located
cd /d "%~dp0"
echo Working folder: %CD%
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop';" ^
  "$here = Get-Location;" ^
  "Get-ChildItem -File | ForEach-Object {" ^
  "  $name = $_.BaseName;" ^
  "  if ($name -match '^(?<y>\d{4})(?<m>\d{2})(?<d>\d{2})_') {" ^
  "    $y = $Matches.y; $m = $Matches.m;" ^
  "    if ([int]$m -ge 1 -and [int]$m -le 12) {" ^
  "      $dest = Join-Path $here ($y + '\' + $m);" ^
  "      if (-not (Test-Path $dest)) { Write-Host ('Creating directory: ' + $y + '\' + $m); New-Item -ItemType Directory -Path $dest | Out-Null }" ^
  "      Write-Host ('Copying: ' + $_.Name + '  -->  ' + $y + '\' + $m);" ^
  "      Copy-Item -LiteralPath $_.FullName -Destination $dest -Force" ^
  "    } else {" ^
  "      Write-Host ('Skipping (bad month): ' + $_.Name)" ^
  "    }" ^
  "  } else {" ^
  "    Write-Host ('Skipping (no date prefix): ' + $_.Name)" ^
  "  }" ^
  "}"

echo.
echo Done.
