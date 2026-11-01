param([string]$Python = 'python', [int]$Jobs = 6, [string]$ToolchainPrefix = '')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$prefix = if ($ToolchainPrefix) { (Resolve-Path -LiteralPath $ToolchainPrefix).Path } else { Join-Path $root '.tools/msys2/ucrt64' }
$env:SWORDIGO_TOOLCHAIN_PREFIX = $prefix
$env:PATH = "$prefix/bin;$env:PATH"
if (-not (Test-Path "$prefix/bin/cmake.exe")) {
    throw 'Run python tools/windows/bootstrap.py first.'
}
$pythonExe = (Get-Command $Python -ErrorAction Stop).Source
& "$prefix/bin/cmake.exe" -S $root -B "$root/build-win64" -G Ninja `
    '-DCMAKE_BUILD_TYPE=Release' '-DCMAKE_POLICY_VERSION_MINIMUM=3.5' `
    '-DSWORDIGO_CLIENT_ONLY=ON' '-DSWORDIGO_CLIENT_TESTS=ON' '-DSWORDIGO_BUILD_SRE=ON' `
    "-DCMAKE_PREFIX_PATH=$prefix" "-DPython3_EXECUTABLE=$pythonExe" `
    "-DSWORDIGO_GUEST_CC=$prefix/bin/clang.exe" -Wno-dev
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
& "$prefix/bin/cmake.exe" --build "$root/build-win64" --target swordfare_boot client_preflight_test client_abi_test client_jit_test client_io_test client_program_state_test client_input_test --parallel $Jobs
if ($LASTEXITCODE) { throw 'Build failed.' }
& "$prefix/bin/ctest.exe" --test-dir "$root/build-win64" --output-on-failure
if ($LASTEXITCODE) { throw 'Client tests failed.' }
& "$PSScriptRoot/launcher/build_launcher.ps1"
& $pythonExe "$PSScriptRoot/package.py"
if ($LASTEXITCODE) { throw 'Packaging failed.' }
