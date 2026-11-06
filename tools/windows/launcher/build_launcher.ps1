# Builds tools/windows/launcher/bin/Ricasso.exe (the no-terminal starter).
# Requires Go 1.22+ (https://go.dev/dl/). Dependencies are vendored; no download needed.
param([string]$Go = 'go')
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$out = Join-Path $here 'bin/Ricasso.exe'
$goCmd = Get-Command $Go -ErrorAction SilentlyContinue
if (-not $goCmd) {
    if (Test-Path $out) {
        Write-Warning "Go not found - reusing existing $out"
        exit 0
    }
    throw 'Go 1.22+ is required to build the starter Ricasso.exe (https://go.dev/dl/).'
}
Push-Location $here
try {
    $env:CGO_ENABLED = '0'
    $env:GOFLAGS = '-mod=vendor'
    & $goCmd.Source test ./importer
    if ($LASTEXITCODE) { throw 'Starter tests failed.' }
    $env:GOOS = 'windows'
    $env:GOARCH = 'amd64'
    & $goCmd.Source build -trimpath -ldflags '-s -w -H windowsgui' -o $out .
    if ($LASTEXITCODE) { throw 'Starter build failed.' }
} finally {
    Pop-Location
    Remove-Item Env:GOOS, Env:GOARCH, Env:CGO_ENABLED, Env:GOFLAGS -ErrorAction SilentlyContinue
}
Write-Host "Starter: $out"
