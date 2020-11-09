[CmdletBinding()]
param ()

$ErrorActionPreference = "Stop"

$this_script = $MyInvocation.MyCommand.Definition
$tools_dir = Split-Path -Parent $this_script
$root_dir = Split-Path -Parent $tools_dir
$build_dir = Join-Path $root_dir "_build"
New-Item -ItemType Container $build_dir -ErrorAction Ignore

$local_tgz = Join-Path $build_dir "openssl.tgz"

# This is the path to the release static vs2019 x64 build of OpenSSL in bintray
$conan_ssl_path = "_/openssl/1.1.1h/_/7098aea4e4f2247cc9b5dcaaa1ebddbe/package/a79a557254fabcb77849dd623fed97c9c5ab7651/141ef2c6711a254707ba1f7f4fd07ad4"
$openssl_url = "https://dl.bintray.com/conan/conan-center/$conan_ssl_path/conan_package.tgz"

Write-Host "Downloading OpenSSL for Windows"
Invoke-WebRequest `
    -Uri $openssl_url `
    -OutFile $local_tgz

$openssl_tree = Join-Path $root_dir "external/OpenSSL"
Write-Host "Expanding OpenSSL archive..."
Remove-Item $openssl_tree -Recurse -Force -ErrorAction Ignore
New-Item $openssl_tree -ItemType Container | Out-Null
& cmake -E chdir $openssl_tree cmake -E tar xf $local_tgz
if ($LASTEXITCODE) {
    throw "Archive expansion failed"
}
