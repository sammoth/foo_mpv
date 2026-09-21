[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('x86-mpv', 'x64-mpv')]
    [string] $Triplet
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$vcpkgRoot = Join-Path $repositoryRoot 'vcpkg'
$vcpkg = Join-Path $vcpkgRoot 'vcpkg.exe'
$dependencyFile = Join-Path $repositoryRoot 'vcpkg.txt'
$tripletDirectory = Join-Path $repositoryRoot 'triplet'
$overlayRoot = $null

if (-not (Test-Path -LiteralPath $vcpkg)) {
    & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) {
        throw 'Failed to bootstrap vcpkg.'
    }
}

try {
    $arguments = @(
        'install'
        "@$dependencyFile"
        "--triplet=$Triplet"
        "--overlay-triplets=$tripletDirectory"
    )

    if ($Triplet -eq 'x86-mpv') {
        # dav1d rejects dynamic-CRT x86 builds in its port metadata, although
        # this component has successfully used that configuration for years.
        # Patch a temporary overlay rather than modifying the vcpkg submodule.
        $overlayRoot = Join-Path ([IO.Path]::GetTempPath()) (
            'foo-mpv-vcpkg-overlay-' + [Guid]::NewGuid().ToString('N'))
        $overlayPort = Join-Path $overlayRoot 'dav1d'
        New-Item -ItemType Directory -Path $overlayPort | Out-Null
        Copy-Item -Path (Join-Path $vcpkgRoot 'ports\dav1d\*') `
            -Destination $overlayPort -Recurse

        $portManifest = Join-Path $overlayPort 'vcpkg.json'
        $manifest = Get-Content -LiteralPath $portManifest -Raw
        $restriction = '!(windows & x86 & !static)'
        if (-not $manifest.Contains($restriction)) {
            throw 'The expected dav1d x86 support restriction was not found.'
        }
        $manifest.Replace($restriction, 'true') |
            Set-Content -LiteralPath $portManifest -NoNewline
        $arguments += "--overlay-ports=$overlayRoot"
    }

    & $vcpkg @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg failed for triplet $Triplet."
    }
}
finally {
    if ($overlayRoot -and (Test-Path -LiteralPath $overlayRoot)) {
        Remove-Item -LiteralPath $overlayRoot -Recurse -Force
    }
}
