[CmdletBinding()]
param ()

$ErrorActionPreference = "Stop"

$this_script = $MyInvocation.MyCommand.Definition
$tools_dir = Split-Path -Parent $this_script
$root_dir = Split-Path -Parent $tools_dir
$build_dir = Join-Path $root_dir "_build"
New-Item -ItemType Container $build_dir -ErrorAction Ignore

$local_tgz = Join-Path $tools_dir "windows-openssl.tgz"

$openssl_tree = Join-Path $root_dir "external/OpenSSL"
Write-Host "Expanding OpenSSL archive..."
Remove-Item $openssl_tree -Recurse -Force -ErrorAction Ignore
New-Item $openssl_tree -ItemType Container | Out-Null
& cmake -E chdir $openssl_tree cmake -E tar xf $local_tgz
if ($LASTEXITCODE) {
    throw "Archive expansion failed"
}
