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
$expectedEnglishScriptHash = 'E82DC824449381B9A132AA092740C4169EB3D77A820D969046B74CA1D9DF4ADF'
$expectedDlls = @('SDL3.dll', 'SDL3_ttf.dll')
$newRuntimeFiles = @('SchoolDays.exe') + $expectedDlls
$candidate = [System.IO.Path]::GetFullPath($CandidateRelease)
$target = [System.IO.Path]::GetFullPath($TargetRelease)
$backup = [System.IO.Path]::GetFullPath($BackupDirectory)

function Require-OrdinaryRuntimeFile([string] $path) {
    $item = Get-Item -LiteralPath $path -Force -ErrorAction Stop
    if ($item -isnot [IO.FileInfo] -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Runtime payload cannot be a directory or reparse point: $path"
    }
}

if ($candidate -eq $target -or (Test-Path -LiteralPath $backup)) {
    throw 'Candidate and target must be distinct; backup destination must not exist.'
}
foreach ($pair in @(@($candidate, $target), @($candidate, $backup), @($target, $backup))) {
    $left = $pair[0].TrimEnd('\', '/')
    $right = $pair[1].TrimEnd('\', '/')
    if ($left.Equals($right, [StringComparison]::OrdinalIgnoreCase) -or
        $left.StartsWith($right + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
        $right.StartsWith($left + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Candidate, target and backup must be separate, non-nested directories.'
    }
}
foreach ($folder in @($candidate, $target)) {
    if (-not (Test-Path -LiteralPath $folder -PathType Container)) {
        throw "Release directory missing: $folder"
    }
}
if (Get-CimInstance Win32_Process -Filter "Name = 'SchoolDays.exe'") {
    throw 'A SchoolDays.exe process is active; do not replace a running build.'
}
foreach ($name in $newRuntimeFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $candidate $name) -PathType Leaf)) {
        throw "Candidate runtime file missing: $name"
    }
    Require-OrdinaryRuntimeFile (Join-Path $candidate $name)
}
$candidateDlls = @(Get-ChildItem -LiteralPath $candidate -Filter '*.dll' -File -Force |
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
if ((Get-FileHash -LiteralPath (Join-Path $packs 'Script.GPK') -Algorithm SHA256).Hash -ne
    $expectedEnglishScriptHash) {
    throw 'Original retail English Script.GPK SHA256 differs from the authenticated build.'
}
foreach ($path in @('Save', 'translations', 'display-options/System/Option')) {
    if (-not (Test-Path -LiteralPath (Join-Path $target $path))) {
        throw "Required destination user content missing: $path"
    }
}
$oldFiles = @((Get-ChildItem -LiteralPath $target -Filter '*.dll' -File -Force) |
    Select-Object -ExpandProperty Name)
if (Test-Path -LiteralPath (Join-Path $target 'SchoolDays.exe')) {
    $oldFiles += 'SchoolDays.exe'
}
foreach ($name in $oldFiles) {
    Require-OrdinaryRuntimeFile (Join-Path $target $name)
}
if ($DryRun) {
    Write-Output "TWO_DLL_DEPLOY_PREFLIGHT_PASS candidate=$candidate target=$target"
    Write-Output "Files that would be backed up: $($oldFiles -join ', ')"
    return
}

New-Item -ItemType Directory -Path $backup -Force | Out-Null
# The recovery set is the entire original runtime, including DLLs removed to
# reach the two-DLL layout. Verify each backup before changing any target byte.
$originalHashes = @{}
foreach ($name in $oldFiles) {
    Copy-Item -LiteralPath (Join-Path $target $name) -Destination (Join-Path $backup $name)
    $originalHashes[$name] = (Get-FileHash -LiteralPath (Join-Path $target $name) -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath (Join-Path $backup $name) -Algorithm SHA256).Hash -ne
        $originalHashes[$name]) {
        throw "Backup verification failed for $name; target was not modified."
    }
}
try {
    foreach ($name in $newRuntimeFiles) {
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
    $actualDlls = @(Get-ChildItem -LiteralPath $target -Filter '*.dll' -File -Force |
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
    $rollbackErrors = New-Object 'System.Collections.Generic.List[string]'
    # Remove every candidate file that did not exist in the original target.
    # Otherwise a failed publish can leave one newly added DLL behind, falsely
    # claiming that the old release was restored.
    foreach ($name in $newRuntimeFiles) {
        if ($oldFiles -contains $name) { continue }
        $path = Join-Path $target $name
        try {
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
        } catch {
            $rollbackErrors.Add("Could not remove newly copied $name`: $_")
        }
    }
    foreach ($name in $oldFiles) {
        try {
            $restored = Join-Path $target $name
            Copy-Item -LiteralPath (Join-Path $backup $name) -Destination $restored -Force
            if ((Get-FileHash -LiteralPath $restored -Algorithm SHA256).Hash -ne $originalHashes[$name]) {
                throw 'Restored runtime SHA256 differs from backup'
            }
        } catch {
            $rollbackErrors.Add("Could not restore $name`: $_")
        }
    }
    if ($rollbackErrors.Count -ne 0) {
        throw "Two-DLL deployment failed: $failure. ROLLBACK INCOMPLETE; original backups remain at $backup. $($rollbackErrors -join '; ')"
    }
    throw "Two-DLL release deployment rolled back completely: $failure"
}
