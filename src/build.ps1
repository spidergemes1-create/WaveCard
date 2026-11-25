param(
 [Parameter(Mandatory)][string]$CompilerBin,
 [Parameter(Mandatory)][string]$ObsSource,
 [Parameter(Mandatory)][string]$ObsInstall
)
$ErrorActionPreference = 'Stop'
$bin=(Resolve-Path $CompilerBin).Path
$include=(Resolve-Path (Join-Path $ObsSource 'libobs')).Path
$obs=(Resolve-Path (Join-Path $ObsInstall 'bin/64bit/obs.dll')).Path
$project=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build=New-Item -Type Directory -Force (Join-Path $project build)
$out=Join-Path $project 'ya-music'
Push-Location $build
try {
 Set-Content obsconfig.h '#pragma once'
 & "$bin/gendef.exe" $obs
 if ($LASTEXITCODE) {throw 'gendef failed'}
 & "$bin/dlltool.exe" -d obs.def -l libobs.a
 if ($LASTEXITCODE) {throw 'dlltool failed'}
 & "$bin/g++.exe" -std=c++17 -Os -s -static -shared "-I$include" "-I$($build.FullName)" "$PSScriptRoot/plugin.cpp" libobs.a -o "$out/ya-music.dll" -lruntimeobject -lshcore -luuid -lgdi32 -lole32 -lgdiplus -lshlwapi
 if ($LASTEXITCODE) {throw 'Plugin compile failed'}
 $rc=@(
 '1 24 "' + ($PSScriptRoot.Replace('\','/')) + '/setup.manifest"'
 '101 RCDATA "' + ($out.Replace('\','/')) + '/ya-music.dll"'
 '102 RCDATA "' + ($out.Replace('\','/')) + '/data/note.svg"'
 )
 Set-Content setup.rc ($rc -join [Environment]::NewLine)
 & "$bin/windres.exe" -i setup.rc -o setup.o
 if ($LASTEXITCODE) {throw 'Resource compile failed'}
 & "$bin/g++.exe" -std=c++17 -Os -s -static -municode -mwindows "$PSScriptRoot/setup.cpp" setup.o -o "$project/WaveCard-Setup.exe" -lshell32 -lole32 -luuid -lcomctl32 -lgdiplus -lgdi32
 if ($LASTEXITCODE) {throw 'Installer compile failed'}
} finally {Pop-Location}

