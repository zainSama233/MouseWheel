param([string]$Executable="$PSScriptRoot/../dist/MouseWheel-Portable.exe")
$ErrorActionPreference='Stop'
if (!(Test-Path $Executable)) { throw 'Portable EXE has not been built' }
$root=Split-Path $PSScriptRoot -Parent
$case=Join-Path $root ('.tmp/portable-test-'+[guid]::NewGuid().ToString()+'/中文 空格')
New-Item -ItemType Directory -Force $case | Out-Null
$target=Join-Path $case 'MouseWheel.exe'
Copy-Item $Executable $target
$originalPath=$env:PATH
try {
    $env:PATH="$env:SystemRoot/System32;$env:SystemRoot"
    foreach($attempt in 1..2) {
        $process=Start-Process -FilePath $target -ArgumentList '--smoke-test' -PassThru -WindowStyle Hidden
        if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'Portable launch timed out' }
        if ($process.ExitCode -ne 0) { throw "Portable launch failed: $($process.ExitCode)" }
        if (!(Test-Path "$case/config.json")) { throw 'Configuration was not saved beside the launcher' }
        $hash=(Get-FileHash "$case/config.json").Hash
        if ($attempt -eq 1) { $initialHash=$hash } elseif ($hash -ne $initialHash) { throw 'Second launch changed user config' }
    }
    $runtime=Get-ChildItem "$case/.mousewheel" -Directory | Select-Object -First 1
    if (!(Test-Path "$($runtime.FullName)/platforms/qwindows.dll")) { throw 'Qt platform plugin missing' }
    Write-Output 'PASS: isolated single EXE, Unicode path, cache reuse, configuration preservation'
} finally { $env:PATH=$originalPath }
