<#
Deploy a verified two-DLL School Days Windows build over the existing L4
Release without altering Packs, translations, saves or display options.
The input must already have been compiled and tested in a separate build.
#>
param(
    [Parameter(Mandatory = $true)] [string] $CandidateRelease,
    [Parameter(Mandatory = $true)] [string] $TargetRelease,
    [Parameter(Mandatory = $true)] [string] $BackupDirectory,
    [switch] $DryRun
)

$ErrorActionPreference = 'Stop'
$expectedPackHash = '772B501FC70AE89E52861378A99FF1B27D3C47EB20B2473836591069143EDB64'
$expectedDlls = @('SDL3.dll', 'SDL3_ttf.dll')
$candidate = [System.IO.Path]::GetFullPath($CandidateRelease)
$target = [System.IO.Path]::GetFullPath($TargetRelease)
$backup = [System.IO.Path]::GetFullPath($BackupDirectory)

if ($candidate -eq $target -or (Test-Path -LiteralPath $backup)) {
    throw 'Candidate and target must be distinct; backup destination must not exist.'
}
foreach ($folder in @($candidate, $target)) {
    if (-not (Test-Path -LiteralPath $folder -PathType Container)) {
        throw "Release directory missing: $folder"
    }
}
if (Get-CimInstance Win32_Process -Filter "Name = 'SchoolDays.exe'") {
    throw 'A SchoolDays.exe process is active; do not replace a running build.'
}
foreach ($name in @('SchoolDays.exe') + $expectedDlls) {
    if (-not (Test-Path -LiteralPath (Join-Path $candidate $name) -PathType Leaf)) {
        throw "Candidate runtime file missing: $name"
    }
}
$candidateDlls = @(Get-ChildItem -LiteralPath $candidate -Filter '*.dll' -File |
    Select-Object -ExpandProperty Name | Sort-Object)
if (($candidateDlls -join '|') -ne (($expectedDlls | Sort-Object) -join '|')) {
    throw "Candidate must ship exactly SDL3.dll and SDL3_ttf.dll; got: $($candidateDlls -join ', ')"
}
$packs = Join-Path $target 'Packs'
if (@(Get-ChildItem -LiteralPath $packs -Filter '*.GPK' -File).Count -ne 30) {
    throw 'Destination must have 29 original GPKs and the mandatory integrated LxH.GPK.'
}
if ((Get-FileHash -LiteralPath (Join-Path $packs 'LxH.GPK') -Algorithm SHA256).Hash -ne
    $expectedPackHash) {
    throw 'Mandatory LxH.GPK SHA256 differs from the authenticated build.'
}
foreach ($path in @('Save', 'translations', 'display-options/System/Option')) {
    if (-not (Test-Path -LiteralPath (Join-Path $target $path))) {
        throw "Required destination user content missing: $path"
    }
}
$oldFiles = @((Get-ChildItem -LiteralPath $target -Filter '*.dll' -File) |
    Select-Object -ExpandProperty Name)
if (Test-Path -LiteralPath (Join-Path $target 'SchoolDays.exe')) {
    $oldFiles += 'SchoolDays.exe'
}
if ($DryRun) {
    Write-Output "TWO_DLL_DEPLOY_PREFLIGHT_PASS candidate=$candidate target=$target"
    Write-Output "Files that would be backed up: $($oldFiles -join ', ')"
    return
}

New-Item -ItemType Directory -Path $backup -Force | Out-Null
foreach ($name in $oldFiles) {
    Copy-Item -LiteralPath (Join-Path $target $name) -Destination (Join-Path $backup $name)
}
try {
    foreach ($name in @('SchoolDays.exe') + $expectedDlls) {
        Copy-Item -LiteralPath (Join-Path $candidate $name) -Destination (Join-Path $target $name) -Force
        if ((Get-FileHash (Join-Path $candidate $name) -Algorithm SHA256).Hash -ne
            (Get-FileHash (Join-Path $target $name) -Algorithm SHA256).Hash) {
            throw "Copied runtime differs from candidate: $name"
        }
    }
    foreach ($name in $oldFiles) {
        if ($name -eq 'SchoolDays.exe' -or $expectedDlls -contains $name) { continue }
        Remove-Item -LiteralPath (Join-Path $target $name)
    }
    $actualDlls = @(Get-ChildItem -LiteralPath $target -Filter '*.dll' -File |
        Select-Object -ExpandProperty Name | Sort-Object)
    if (($actualDlls -join '|') -ne (($expectedDlls | Sort-Object) -join '|')) {
        throw "Deployed DLL list is not exactly two: $($actualDlls -join ', ')"
    }
    if ((Get-FileHash -LiteralPath (Join-Path $packs 'LxH.GPK') -Algorithm SHA256).Hash -ne
        $expectedPackHash) {
        throw 'Integrated pack changed during deployment.'
    }
    Write-Output "TWO_DLL_RELEASE_PUBLISHED target=$target backup=$backup"
} catch {
    $failure = $_
    foreach ($name in $oldFiles) {
        Copy-Item -LiteralPath (Join-Path $backup $name) -Destination (Join-Path $target $name) -Force
    }
    throw "Two-DLL release deployment rolled back: $failure"
}
