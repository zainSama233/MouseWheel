$ErrorActionPreference='Stop'
. "$PSScriptRoot/toolchain.ps1"
$stage=Join-Path $root 'build/portable-payload'
New-Item -ItemType Directory -Force $stage | Out-Null
$package=Join-Path $root 'dist/MouseWheel'
$files=Get-ChildItem $package -File | Where-Object { $_.Extension -eq '.dll' -or $_.Name -in @('MouseWheel.exe','LICENSE','README.md') }
foreach($folder in @('platforms','imageformats','iconengines','styles','tls','networkinformation','licenses')) {
    if(Test-Path "$package/$folder"){ $files+=Get-ChildItem "$package/$folder" -File -Recurse }
}
# Stage with short ASCII source names; destination paths retain the package structure.
$ddf=@('.OPTION EXPLICIT','.Set CabinetNameTemplate=payload.cab','.Set DiskDirectoryTemplate=.','.Set MaxDiskSize=0','.Set CompressionType=LZX','.Set Cabinet=on','.Set Compress=on')
$index=0
foreach($file in $files) {
    $name='file'+$index++;Copy-Item -LiteralPath $file.FullName -Destination "$stage/$name" -Force
    $relative=[IO.Path]::GetRelativePath($package,$file.FullName)
    $ddf+='"'+$name+'" "'+$relative+'"'
}
$ddf | Set-Content "$stage/payload.ddf" -Encoding ascii
Push-Location $stage
try { & makecab.exe /F payload.ddf | Out-File makecab.log; if($LASTEXITCODE){throw 'Cabinet build failed'} } finally { Pop-Location }
$payload=(Join-Path $stage 'payload.cab').Replace('\','/')
& $cmake -S "$root/packaging/windows" -B "$root/build/portable-launcher" -G 'Visual Studio 17 2022' -A x64 "-DPAYLOAD=$payload"
if($LASTEXITCODE){throw 'Launcher configure failed'}
& $cmake --build "$root/build/portable-launcher" --config Release -j 4
if($LASTEXITCODE){throw 'Launcher build failed'}
Copy-Item "$root/build/portable-launcher/Release/MouseWheelPortable.exe" "$root/dist/MouseWheel-Portable.exe" -Force
