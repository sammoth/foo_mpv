[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('i686', 'x86_64')]
    [string] $Architecture,

    [Parameter(Mandatory = $true)]
    [string] $Destination
)

$ErrorActionPreference = 'Stop'
$sevenZip = Get-Command 7z -ErrorAction Stop
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) (
    'foo-mpv-libmpv-' + [Guid]::NewGuid().ToString('N'))
$archive = Join-Path $temporaryRoot 'libmpv.7z'
$extracted = Join-Path $temporaryRoot 'extracted'

try {
    New-Item -ItemType Directory -Path $extracted | Out-Null
    $rss = Invoke-WebRequest -UseBasicParsing `
        -Uri 'https://sourceforge.net/projects/mpv-player-windows/rss?path=/libmpv'

    # Select the non-v3 x64 build so the package works on older x64 CPUs too.
    $pattern = if ($Architecture -eq 'x86_64') {
        '<link>([^<]*mpv-dev-x86_64-[0-9][^<]*\.7z/download)</link>'
    } else {
        '<link>([^<]*mpv-dev-i686-[0-9][^<]*\.7z/download)</link>'
    }
    $match = [regex]::Match($rss.Content, $pattern)
    if (-not $match.Success) {
        throw "Could not find the latest $Architecture libmpv archive."
    }

    $url = [Net.WebUtility]::HtmlDecode($match.Groups[1].Value)
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $archive `
        -UserAgent 'foo_mpv release builder'
    & $sevenZip.Source x $archive "-o$extracted" -y
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to extract the $Architecture libmpv archive."
    }

    $runtime = Get-ChildItem -Path $extracted -Filter 'libmpv-2.dll' -Recurse |
        Select-Object -First 1
    if (-not $runtime) {
        $runtime = Get-ChildItem -Path $extracted -Filter 'mpv-2.dll' -Recurse |
            Select-Object -First 1
    }
    if (-not $runtime) {
        throw "The $Architecture archive did not contain a libmpv runtime DLL."
    }

    $destinationDirectory = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    Copy-Item -LiteralPath $runtime.FullName -Destination $Destination -Force
    Write-Output "Downloaded $Architecture libmpv from $url"
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
