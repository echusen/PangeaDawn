[CmdletBinding()]
param(
    [ValidateSet('Check', 'Protect')]
    [string]$Action = 'Check'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$vendorRoots = @(
    'Plugins/ACFUAsce5ab7c1439afbV5',
    'Plugins/ACFUltimate',
    'Content/FullSample',
    'Content/AnimationLibrary'
)

function Get-GitOutput([string]$Repository, [string[]]$GitArgs) {
    $output = @(& git -C $Repository @GitArgs)
    if ($LASTEXITCODE -ne 0) { throw "Git failed in $Repository" }
    return $output
}

# First validate every baseline. Never protect dirty files and call them stock.
$dirty = @()
$files = @()
$summary = @()
foreach ($relative in $vendorRoots) {
    $directory = [IO.Path]::GetFullPath((Join-Path $projectRoot $relative))
    if (-not $directory.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Vendor path escaped the workspace: $directory"
    }
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) { continue }

    $dirty += Get-GitOutput $projectRoot @('status', '--porcelain=v1', '--untracked-files=all', '--', $relative)
    # Nested repos/submodules own their history; the superproject status alone
    # does not describe their changed files or an unexpected checked-out commit.
    if (Test-Path -LiteralPath (Join-Path $directory '.git')) {
        $dirty += Get-GitOutput $directory @('status', '--porcelain=v1', '--untracked-files=all')
    }

    $authored = @(Get-ChildItem -LiteralPath $directory -Recurse -File -Force |
        Where-Object {
            $local = $_.FullName.Substring($directory.Length + 1)
            $local -notmatch '(^|[\\/])(Binaries|Intermediate|Saved|DerivedDataCache|\.git)([\\/]|$)'
        })
    $files += $authored
    $summary += [pscustomobject]@{ Root = $relative; AuthoredFiles = $authored.Count }
}
$dirty = @($dirty | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
if ($dirty.Count -gt 0) {
    $dirty | ForEach-Object { Write-Warning $_ }
    throw 'Vendor baseline is dirty. Restore authorized changes or complete the explicit vendor update first.'
}

if ($Action -eq 'Protect') {
    foreach ($file in $files) {
        if (-not $file.IsReadOnly) { $file.IsReadOnly = $true }
    }
}
$writable = @($files | Where-Object {
    -not ((Get-Item -LiteralPath $_.FullName -Force).IsReadOnly)
})
if ($writable.Count -gt 0) {
    $writable | Select-Object -First 10 -ExpandProperty FullName | Write-Warning
    throw "$($writable.Count) vendor authored files are writable. Run -Action Protect on a clean baseline."
}
[pscustomobject]@{
    Action = $Action
    GitClean = $true
    AllAuthoredFilesReadOnly = $true
    ProtectedFiles = $files.Count
    Roots = $summary
    Excluded = @('Binaries', 'Intermediate', 'Saved', 'DerivedDataCache', '.git')
} | ConvertTo-Json -Depth 4
