param([switch]$Package)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/toolchain.ps1"
& $cmake -S $root -B "$root/build" -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$qt" -DBUILD_APP=ON
if ($LASTEXITCODE) { throw 'CMake configure failed' }
& $cmake --build "$root/build" --config Release -j 4
if ($LASTEXITCODE) { throw 'Build failed' }
if ($Package) {
    & $cmake --install "$root/build" --config Release --prefix "$root/dist/MouseWheel"
    if ($LASTEXITCODE) { throw 'Install failed' }
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $vs = & $vswhere -latest -products '*' -property installationPath
    $env:VCINSTALLDIR = "$vs/VC/"
    & "$qt/bin/windeployqt.exe" --release --no-translations --no-opengl-sw --no-system-d3d-compiler --no-compiler-runtime "$root/dist/MouseWheel/MouseWheel.exe"
    if ($LASTEXITCODE) { throw 'Qt deployment failed' }
    $crt = Get-ChildItem "$vs/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT" -Directory | Sort-Object FullName -Descending | Select-Object -First 1
    if (!$crt) { throw "VC runtime directory not found" }
    Copy-Item "$($crt.FullName)/*.dll" "$root/dist/MouseWheel/"
    Copy-Item "$root/README.md" "$root/dist/MouseWheel/README.md"
    Copy-Item "$root/licenses" "$root/dist/MouseWheel/" -Recurse -Force
    Copy-Item "$qt/sbom/qtbase-$($versions.qt).spdx.json" "$root/dist/MouseWheel/licenses/"
    Copy-Item "$root/toolchain.json" "$root/dist/MouseWheel/"
    Copy-Item "$root/docs" "$root/dist/MouseWheel/" -Recurse -Force
    foreach ($directory in @("src","tests","scripts")) { Copy-Item "$root/$directory" "$root/dist/MouseWheel/" -Recurse -Force }
    Copy-Item "$root/CMakeLists.txt" "$root/dist/MouseWheel/"
    $contents = Get-ChildItem "$root/dist/MouseWheel" | Where-Object Name -NotIn @("config.json","config.json.lock")
    Compress-Archive -Path $contents.FullName -DestinationPath "$root/dist/MouseWheel-windows-x64.zip" -Force
}
