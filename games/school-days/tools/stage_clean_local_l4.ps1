<#
Prepare a private, clean, install-like Windows folder from a verified L4 Release.

Default invocation is a read-only inventory and full SHA-256 preflight.
Creation requires both -Create and the SHA-256 of the final EXE approved by
the release owner. Never upload the result: it contains commercial retail GPKs.
#>
param(
    [string] $SourceRelease = '',
    [Parameter(Mandatory = $true)] [string] $Destination,
    [switch] $Create,
    [string] $ApprovedExeSha256 = '',
    [switch] $QuickDryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $SourceRelease) {
    $SourceRelease = Join-Path $PSScriptRoot '../../../build/school-days-dev-control4/games/school-days/Release'
}
$source = [IO.Path]::GetFullPath($SourceRelease).TrimEnd('\', '/')
$destinationPath = [IO.Path]::GetFullPath($Destination).TrimEnd('\', '/')
$pathComparison = [StringComparison]::OrdinalIgnoreCase
$retailNames = @(
    'BGM.GPK', 'Event00.GPK', 'Event01.GPK', 'Event02.GPK',
    'Event03.GPK', 'Event04.GPK', 'Event05.GPK', 'Ini.GPK',
    'Movie00.GPK', 'Movie01.GPK', 'Movie02.GPK', 'Movie03.GPK',
    'Movie04.GPK', 'Movie05.GPK', 'Script.GPK',
    'Se00.GPK', 'Se01.GPK', 'Se02.GPK', 'Se03.GPK',
    'Se04.GPK', 'Se05.GPK', 'SysSe.GPK', 'System.gpk',
    'Voice00.GPK', 'Voice01.GPK', 'Voice02.GPK',
    'Voice03.GPK', 'Voice04.GPK', 'Voice05.GPK'
)
$expectedDllNames = @('SDL3.dll', 'SDL3_ttf.dll')
$expectedTranslations = @(
    'en-lxh.json', 'en-lxh-reviewed-identities.json',
    'es.json', 'es-lxh.json', 'es-lxh-reviewed-identities.json',
    'es-reviewed-identities.json', 'zh-CN.json', 'zh-CN-lxh.json'
)
$expectedLxhHash = '772B501FC70AE89E52861378A99FF1B27D3C47EB20B2473836591069143EDB64'
$expectedEnglishScriptHash = 'E82DC824449381B9A132AA092740C4169EB3D77A820D969046B74CA1D9DF4ADF'
$entries = New-Object 'System.Collections.Generic.List[object]'

function Require-Directory([string] $path) {
    if (-not (Test-Path -LiteralPath $path -PathType Container)) {
        throw "Required source directory missing: $path"
    }
}

function Ensure-NotReparse([IO.FileSystemInfo] $item) {
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Refusing reparse point in payload: $($item.FullName)"
    }
}

function Add-PayloadFile([string] $path, [string] $relative, [string] $role) {
    $item = Get-Item -LiteralPath $path -ErrorAction Stop
    if ($item -isnot [IO.FileInfo]) { throw "Expected ordinary file: $path" }
    Ensure-NotReparse $item
    if ($relative -match '(^|[\\/])\.\.([\\/]|$)' -or [IO.Path]::IsPathRooted($relative)) {
        throw "Unsafe relative path: $relative"
    }
    $sha = if ($QuickDryRun -and $role -eq 'retail-gpk' -and $item.Name -ne 'Script.GPK') {
        'NOT-CHECKED-QUICK-DRY-RUN'
    } else {
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToUpperInvariant()
    }
    $entries.Add([pscustomobject]@{
        relativePath = ($relative -replace '\\', '/')
        role = $role
        bytes = [long] $item.Length
        sha256 = $sha
        sourcePath = $item.FullName
    })
}

function Add-PayloadTree([string] $name, [string] $role) {
    $root = Join-Path $source $name
    Require-Directory $root
    Ensure-NotReparse (Get-Item -LiteralPath $root)
    foreach ($dir in @(Get-ChildItem -LiteralPath $root -Recurse -Directory -Force)) {
        Ensure-NotReparse $dir
    }
    foreach ($file in @(Get-ChildItem -LiteralPath $root -Recurse -File -Force | Sort-Object FullName)) {
        Ensure-NotReparse $file
        $relative = "$name/$($file.FullName.Substring($root.Length).TrimStart('\', '/'))"
        $relative = $relative -replace '\\', '/'
        if ($name -eq 'assets' -and $file.Extension -notin @('.glsl', '.ico', '.jpg', '.jpeg', '.png', '.webp', '.bmp', '.ttf', '.otf', '.txt', '.json')) {
            throw "Unexpected asset extension; review before packaging: $($file.FullName)"
        }
        if ($name -eq 'display-options') {
            if ($file.Extension -notin @('.png', '.cmap', '.json', '.txt') -or
                ($relative -notmatch '^display-options/System/Option/' -and
                 $relative -notin @('display-options/README.txt', 'display-options/display_options_controls.json'))) {
                throw "Unexpected display overlay content; review before packaging: $relative"
            }
        }
        if ($name -eq 'third-party-licenses' -and $file.Extension -ne '.txt') {
            throw "Unexpected license artifact; review before packaging: $relative"
        }
        Add-PayloadFile $file.FullName $relative $role
    }
}

Require-Directory $source
Ensure-NotReparse (Get-Item -LiteralPath $source)
if (Test-Path -LiteralPath $destinationPath) {
    throw "Staging destination already exists; choose a NEW directory: $destinationPath"
}
if ($destinationPath.Equals($source, $pathComparison) -or
    $destinationPath.StartsWith($source + [IO.Path]::DirectorySeparatorChar, $pathComparison) -or
    $source.StartsWith($destinationPath + [IO.Path]::DirectorySeparatorChar, $pathComparison)) {
    throw 'Staging destination and source must be separate, non-nested directories.'
}
$ancestor = Split-Path -Parent $destinationPath
while ($ancestor -and (Test-Path -LiteralPath $ancestor)) {
    Ensure-NotReparse (Get-Item -LiteralPath $ancestor)
    $parent = Split-Path -Parent $ancestor
    if (-not $parent -or $parent -eq $ancestor) { break }
    $ancestor = $parent
}
$packsPath = Join-Path $source 'Packs'
Require-Directory $packsPath
$packsItem = Get-Item -LiteralPath $packsPath
# The existing L4 Release may contain a junction to the untouched retail Packs.
# Only the 30 individual .GPK file bytes are copied; never copy the junction.
if (($packsItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
    if ($packsItem.LinkType -ne 'Junction') {
        throw "Unexpected Packs link type: $($packsItem.LinkType)"
    }
    $target = [string] ($packsItem.Target | Select-Object -First 1)
    if (-not $target) { throw 'Packs junction has no target.' }
    $resolvedTarget = [IO.Path]::GetFullPath($target).TrimEnd('\', '/')
    if ($destinationPath.Equals($resolvedTarget, $pathComparison) -or
        $destinationPath.StartsWith($resolvedTarget + [IO.Path]::DirectorySeparatorChar, $pathComparison) -or
        $resolvedTarget.StartsWith($destinationPath + [IO.Path]::DirectorySeparatorChar, $pathComparison)) {
        throw 'Destination must not intersect the original Packs junction target.'
    }
    Write-Output "READ_ONLY_PACK_SOURCE junction=$resolvedTarget"
}
$packFiles = @(Get-ChildItem -LiteralPath $packsPath -File -Force | Where-Object { $_.Extension -ieq '.gpk' })
$actualPackNames = @( $packFiles | ForEach-Object { $_.Name } | Sort-Object )
$expectedPackNames = @( @($retailNames) + @('LxH.GPK') | Sort-Object )
if (($actualPackNames -join '|') -ine ($expectedPackNames -join '|')) {
    throw "Expected 29 retail GPKs + LxH.GPK, found: $($actualPackNames -join ', ')"
}
if (@(Get-ChildItem -LiteralPath $packsPath -Directory -Force).Count -gt 0) {
    throw 'Packs contains unexpected subdirectories; refusing recursive payload copy.'
}
$localDllNames = @(Get-ChildItem -LiteralPath $source -File -Filter '*.dll' | ForEach-Object { $_.Name } | Sort-Object)
if (($localDllNames -join '|') -ine ((@($expectedDllNames | Sort-Object)) -join '|')) {
    throw "Two-DLL build required; found: $($localDllNames -join ', ')"
}

Add-PayloadFile (Join-Path $source 'SchoolDays.exe') 'SchoolDays.exe' 'runtime'
$exeSha = $entries[0].sha256
foreach ($dll in $expectedDllNames) {
    Add-PayloadFile (Join-Path $source $dll) $dll 'runtime-dll'
}

foreach ($packName in $expectedPackNames) {
    $kind = if ($packName -ieq 'LxH.GPK') { 'integrated-lxh-gpk' } else { 'retail-gpk' }
    Add-PayloadFile (Join-Path $packsPath $packName) "Packs/$packName" $kind
    $last = $entries[$entries.Count - 1]
    if ($packName -ieq 'LxH.GPK' -and $last.sha256 -ne $expectedLxhHash) {
        throw "Integrated LxH.GPK SHA256 mismatch: $($last.sha256)"
    }
    if ($packName -ieq 'Script.GPK' -and $last.sha256 -ne $expectedEnglishScriptHash) {
        throw "Retail English Script.GPK SHA256 mismatch: $($last.sha256)"
    }
}

Add-PayloadTree 'assets' 'engine-and-launcher-assets'
$launcherHero = Join-Path $source 'assets/SchoolDaysHQ-Launcher.png'
if (-not (Test-Path -LiteralPath $launcherHero -PathType Leaf)) {
    if ($Create) {
        throw 'Final launcher hero assets/SchoolDaysHQ-Launcher.png is missing from Release; do not stage yet.'
    }
    Write-Warning 'Final launcher hero assets/SchoolDaysHQ-Launcher.png is not in Release yet; dry-run is preliminary.'
}
Add-PayloadTree 'display-options' 'game-option-artwork'
Add-PayloadTree 'third-party-licenses' 'license-notices'
$gameRoot = Join-Path $source 'game-assets'
Require-Directory $gameRoot
foreach ($name in @('school-days-hq.ktnroute', 'styles.ass')) {
    Add-PayloadFile (Join-Path $gameRoot $name) "game-assets/$name" 'game-routing-and-subtitles'
}
$translationRoot = Join-Path $source 'translations'
Require-Directory $translationRoot
$translationNames = @(Get-ChildItem -LiteralPath $translationRoot -File -Force | Where-Object { $_.Extension -ieq '.json' } | ForEach-Object { $_.Name } | Sort-Object)
if (($translationNames -join '|') -ine ((@($expectedTranslations | Sort-Object)) -join '|')) {
    throw "Unexpected or missing translation JSON (protect private locales): $($translationNames -join ', ')"
}
if (@(Get-ChildItem -LiteralPath $translationRoot -Directory -Force).Count -ne 0) {
    throw 'Unexpected translations subdirectory; review before packaging.'
}
foreach ($name in $expectedTranslations) {
    Add-PayloadFile (Join-Path $translationRoot $name) "translations/$name" 'translation-and-review-metadata'
}

if ($Create) {
    if ($QuickDryRun) { throw '-QuickDryRun is for read-only inventory only.' }
    if ($ApprovedExeSha256 -notmatch '^[0-9a-fA-F]{64}$') {
        throw '-Create requires the final EXE SHA256 explicitly approved by prime.'
    }
    if ($exeSha -ne $ApprovedExeSha256.ToUpperInvariant()) {
        throw "EXE changed or is not approved: got $exeSha, expected $ApprovedExeSha256"
    }
    if (Get-Process -Name SchoolDays -ErrorAction SilentlyContinue) {
        throw 'SchoolDays.exe is active; stop it before creating staging.'
    }
}

$totalBytes = ($entries | Measure-Object -Property bytes -Sum).Sum
Write-Output "PREFLIGHT_PASS mode=$(if($Create){'CREATE'}elseif($QuickDryRun){'QUICK_DRY_RUN'}else{'FULL_DRY_RUN'})"
Write-Output "EXE_SHA256=$exeSha"
Write-Output "GPK_COUNT=$($expectedPackNames.Count) (29 retail + mandatory LxH.GPK)"
Write-Output "LOCAL_DLLS=$($localDllNames -join ', ')"
Write-Output "PAYLOAD_FILES=$($entries.Count) PAYLOAD_BYTES=$totalBytes"
Write-Output "DESTINATION_NEW=$destinationPath"
Write-Output 'EXCLUDED: Save/, Config.DAT, settings.json, language.txt, Kotonoha.ini, shader-paths.txt, logs, PDB/LIB, old LxH.mode/overlay, QA captures.'
if (-not $Create) {
    Write-Output 'DRY_RUN_ONLY: no destination, saves, retail GPK or configuration was modified.'
    return
}

# After the guarded preflight, create only a brand-new local directory.
New-Item -ItemType Directory -Path $destinationPath -ErrorAction Stop | Out-Null
$incomplete = Join-Path $destinationPath 'INCOMPLETE-DO-NOT-USE.txt'
[IO.File]::WriteAllText($incomplete, 'Local staging in progress. Do not run or distribute.', (New-Object System.Text.UTF8Encoding($false)))
try {
    foreach ($entry in $entries) {
        $relative = $entry.relativePath.Replace('/', [IO.Path]::DirectorySeparatorChar)
        $output = Join-Path $destinationPath $relative
        $parent = Split-Path -Parent $output
        if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
            New-Item -ItemType Directory -Path $parent -Force | Out-Null
        }
        Copy-Item -LiteralPath $entry.sourcePath -Destination $output -ErrorAction Stop
        $outputItem = Get-Item -LiteralPath $output
        Ensure-NotReparse $outputItem
        if ($outputItem.Length -ne $entry.bytes -or
            (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToUpperInvariant() -ne $entry.sha256) {
            throw "Copied payload differs from preflight hash: $($entry.relativePath)"
        }
    }
    # These are relative selections only, never copied user settings.
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    $generated = @{
        'dir.txt' = "Packs`n"
        'route.txt' = "school-days-hq.ktnroute`n"
        'README-LOCAL-STAGING.txt' = "School Days HQ v1.02 / Kotonoha L4 - private local staging`r`nContains 29 commercial retail GPKs and integrated LxH.GPK.`r`nFor personal/local testing only; do not publish/upload the retail packs.`r`nFirst run creates fresh settings and saves. English is the default locale.`r`n"
    }
    foreach ($name in @($generated.Keys | Sort-Object)) {
        $output = Join-Path $destinationPath $name
        [IO.File]::WriteAllText($output, $generated[$name], $utf8)
        $item = Get-Item -LiteralPath $output
        $entries.Add([pscustomobject]@{
            relativePath = $name
            role = 'fresh-install-bootstrap'
            bytes = [long] $item.Length
            sha256 = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToUpperInvariant()
            sourcePath = ''
        })
    }
    if (Test-Path -LiteralPath (Join-Path $destinationPath 'Save')) {
        throw 'Safety invariant violated: Save directory must not be staged.'
    }
    $manifestEntries = @($entries | Sort-Object relativePath | ForEach-Object {
        [ordered]@{ path = $_.relativePath; role = $_.role; bytes = $_.bytes; sha256 = $_.sha256 }
    })
    $manifest = [ordered]@{
        schema = 'school-days-clean-local-staging-v1'
        generatedUtc = [DateTime]::UtcNow.ToString('o')
        localOnly = $true
        exeSha256Approved = $exeSha
        fileCount = $manifestEntries.Count
        files = $manifestEntries
    }
    $manifestPath = Join-Path $destinationPath 'manifest-sha256.json'
    [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 8), $utf8)
    $manifestSha = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToUpperInvariant()
    [IO.File]::WriteAllText((Join-Path $destinationPath 'manifest-sha256.sha256'),
        "$manifestSha  manifest-sha256.json`n", $utf8)
    Remove-Item -LiteralPath $incomplete -ErrorAction Stop
    Write-Output "STAGING_COMPLETE files=$($manifestEntries.Count) manifestSHA256=$manifestSha destination=$destinationPath"
} catch {
    throw "Local staging is INCOMPLETE at $destinationPath; do not use it. Reason: $_"
}
