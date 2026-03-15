$ErrorActionPreference = "Stop"

$buildDir = "build"
if (Test-Path $buildDir) {
    Remove-Item -Recurse -Force $buildDir
}
New-Item -ItemType Directory -Path $buildDir | Out-Null

Set-Location $buildDir

cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=bin ..
cmake --build . --config Release
