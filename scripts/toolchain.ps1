$root = Split-Path $PSScriptRoot -Parent
$versions = Get-Content "$root/toolchain.json" -Raw | ConvertFrom-Json
$cmake = Join-Path $root '.tools/python/Scripts/cmake.exe'
$qtBase = Join-Path $env:SystemDrive 'Qt'
$qt = Join-Path $qtBase "$($versions.qt)/msvc2022_64"
