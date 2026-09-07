param([string]$Pattern = '^(core|config|injection)$')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/toolchain.ps1"
$env:PATH = "$qt/bin;$env:PATH"
& "$root/.tools/python/Scripts/ctest.exe" --test-dir "$root/build" -C Release -R $Pattern --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed' }
