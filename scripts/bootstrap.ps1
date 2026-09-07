$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/toolchain.ps1"
Set-Location $root
New-Item -ItemType Directory -Force '.tools' | Out-Null
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
if (!(Test-Path $vswhere) -or !(& $vswhere -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)) {
    Invoke-WebRequest 'https://aka.ms/vs/17/release/vs_BuildTools.exe' -OutFile '.tools/vs_BuildTools.exe'
    $setup = Start-Process '.tools/vs_BuildTools.exe' -ArgumentList '--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended' -WindowStyle Hidden -Wait -PassThru
    if ($setup.ExitCode -notin 0,3010) { throw "MSVC installation failed: $($setup.ExitCode)" }
}
if (!(Test-Path '.tools/python/Scripts/python.exe')) {
    python -m venv .tools/python
    if ($LASTEXITCODE) { throw 'Python 3.11+ is required on PATH' }
}
& .tools/python/Scripts/python.exe -m pip install "aqtinstall==$($versions.aqtinstall)" "cmake==$($versions.cmake)" "ninja==$($versions.ninja)"
if ($LASTEXITCODE) { throw 'Build dependency installation failed' }
if (!(Test-Path "$qt/lib/cmake/Qt6/Qt6Config.cmake")) {
    & .tools/python/Scripts/python.exe -m aqt install-qt windows desktop $versions.qt win64_msvc2022_64 -O $qtBase --archives qtbase qttools
    if ($LASTEXITCODE) { throw 'Qt installation failed; inspect aqtinstall.log' }
}
