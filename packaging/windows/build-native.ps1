param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$WhisperSource,
    [string]$BuildDirectory = 'build-win-local'
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
$WhisperSource = (Resolve-Path -LiteralPath $WhisperSource).Path
$env:PATH = (Join-Path $QtRoot 'bin') + ';' + $env:PATH
$cmake = (Get-Command cmake -ErrorAction Stop).Source
& $cmake -S $sourceRoot -B (Join-Path $sourceRoot $BuildDirectory) -G Ninja `
    '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_PREFIX_PATH=$QtRoot" `
    "-DCMAKE_C_COMPILER=$QtRoot/bin/clang.exe" "-DCMAKE_CXX_COMPILER=$QtRoot/bin/clang++.exe" `
    "-DFETCHCONTENT_SOURCE_DIR_WHISPER=$WhisperSource" '-DGGML_NATIVE=OFF' `
    '-DGGML_AVX=ON' '-DGGML_AVX2=ON' '-DGGML_FMA=ON' '-DGGML_F16C=ON' '-DGGML_BMI2=ON' '-DGGML_SSE42=ON'
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& $cmake --build (Join-Path $sourceRoot $BuildDirectory) --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
