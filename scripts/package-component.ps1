[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Win32Dll,

    [Parameter(Mandatory = $true)]
    [string] $X64Dll,

    [Parameter(Mandatory = $true)]
    [string] $Win32Libmpv,

    [Parameter(Mandatory = $true)]
    [string] $X64Libmpv,

    [string] $OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'artifacts')
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$oscScript = Join-Path $repositoryRoot 'src\lua\osc.lua'
$inputs = @($Win32Dll, $X64Dll, $Win32Libmpv, $X64Libmpv, $oscScript)
foreach ($inputFile in $inputs) {
    if (-not (Test-Path -LiteralPath $inputFile -PathType Leaf)) {
        throw "Required package input does not exist: $inputFile"
    }
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$stagingRoot = Join-Path $OutputDirectory '.component-staging'
$symbolsRoot = Join-Path $OutputDirectory '.symbols-staging'
$packageZip = Join-Path $OutputDirectory 'foo_mpv.zip'
$package = Join-Path $OutputDirectory 'foo_mpv.fb2k-component'
$symbols = Join-Path $OutputDirectory 'foo_mpv-symbols.zip'

foreach ($path in @($stagingRoot, $symbolsRoot)) {
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}
foreach ($path in @($packageZip, $package, $symbols)) {
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Force
    }
}

New-Item -ItemType Directory -Path (Join-Path $stagingRoot 'mpv') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stagingRoot 'x64\mpv') -Force | Out-Null
Copy-Item -LiteralPath $Win32Dll -Destination (Join-Path $stagingRoot 'foo_mpv.dll')
Get-ChildItem -LiteralPath (Split-Path -Parent $Win32Libmpv) -Filter '*.dll' -File |
    Copy-Item -Destination (Join-Path $stagingRoot 'mpv')
Copy-Item -LiteralPath $oscScript -Destination (Join-Path $stagingRoot 'mpv\osc.lua')
Copy-Item -LiteralPath $X64Dll -Destination (Join-Path $stagingRoot 'x64\foo_mpv.dll')
Get-ChildItem -LiteralPath (Split-Path -Parent $X64Libmpv) -Filter '*.dll' -File |
    Copy-Item -Destination (Join-Path $stagingRoot 'x64\mpv')

Compress-Archive -Path (Join-Path $stagingRoot '*') -DestinationPath $packageZip
Move-Item -LiteralPath $packageZip -Destination $package

$win32Pdb = [IO.Path]::ChangeExtension($Win32Dll, '.pdb')
$x64Pdb = [IO.Path]::ChangeExtension($X64Dll, '.pdb')
if ((Test-Path -LiteralPath $win32Pdb) -and (Test-Path -LiteralPath $x64Pdb)) {
    New-Item -ItemType Directory -Path (Join-Path $symbolsRoot 'Win32') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $symbolsRoot 'x64') -Force | Out-Null
    Copy-Item -LiteralPath $win32Pdb -Destination (Join-Path $symbolsRoot 'Win32\foo_mpv.pdb')
    Copy-Item -LiteralPath $x64Pdb -Destination (Join-Path $symbolsRoot 'x64\foo_mpv.pdb')
    Compress-Archive -Path (Join-Path $symbolsRoot '*') -DestinationPath $symbols
}

Remove-Item -LiteralPath $stagingRoot -Recurse -Force
if (Test-Path -LiteralPath $symbolsRoot) {
    Remove-Item -LiteralPath $symbolsRoot -Recurse -Force
}

Write-Output $package
if (Test-Path -LiteralPath $symbols) {
    Write-Output $symbols
}
