# Package-RoadDelivery.ps1
# 역할: 도로·타일 제작 원본과 납품 작업에서 검증된 결과를 지정된 배포 폴더에 복사한다.

param(
    [Parameter(Mandatory=$true)][string]$Destination
)
$ErrorActionPreference = 'Stop'
$roadProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$roadSource = [IO.Path]::GetFullPath((Join-Path $roadProject 'SourceArt\RoadTiles_20260910'))
$roadOutput = [IO.Path]::GetFullPath($Destination)
# 소스 자신/하위 폴더에 복사해 재귀적으로 늘어나는 경로만 차단한다.
if ($roadOutput.Equals($roadSource, [StringComparison]::OrdinalIgnoreCase) -or
    $roadOutput.StartsWith($roadSource + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Delivery destination must be outside the source directory'
}
if (Test-Path -LiteralPath $roadOutput) { throw 'Destination already exists; refusing to overwrite' }
$roadValidation = Get-Content -LiteralPath (Join-Path $roadSource 'validation_report.json') -Raw | ConvertFrom-Json
if ($roadValidation.status -ne 'PASS') { throw 'Final validation must pass before delivery' }
New-Item -ItemType Directory -Path $roadOutput -Force | Out-Null
foreach ($roadFolder in @('Blends','Exports','Textures','References','Code')) {
    Copy-Item -LiteralPath (Join-Path $roadSource $roadFolder) -Destination $roadOutput -Recurse
}
Get-ChildItem -LiteralPath $roadSource -File | Where-Object Extension -In '.md','.json' |
    Copy-Item -Destination $roadOutput
# The shared MCP console log also contains another task's activity; deliver only
# this job's per-artifact evidence, not that shared process log or temporary caches.
$roadQA = Join-Path $roadSource 'qa'
Get-ChildItem -LiteralPath $roadQA -Recurse -File | Where-Object Extension -In '.png','.json','.fbx','.blend' | ForEach-Object {
    $roadRelative = [IO.Path]::GetRelativePath($roadSource, $_.FullName)
    $roadTarget = [IO.Path]::GetFullPath((Join-Path $roadOutput $roadRelative))
    if (-not $roadTarget.StartsWith($roadOutput + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Evidence target escaped output' }
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($roadTarget)) -Force | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $roadTarget
}
$roadManifest = Get-Content -LiteralPath (Join-Path $roadOutput 'file_manifest.json') -Raw | ConvertFrom-Json
$roadCount = 0
foreach ($roadProperty in $roadManifest.PSObject.Properties) {
    $roadFile = Join-Path $roadOutput $roadProperty.Name
    $roadActualHash = (Get-FileHash -LiteralPath $roadFile -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($roadActualHash -ne $roadProperty.Value.sha256) { throw ('Copied file checksum failed: ' + $roadProperty.Name) }
    $roadCount++
}
$roadResult = [ordered]@{
    status='PASS'; output=$roadOutput; verified_files=$roadCount;
    all_files=@(Get-ChildItem -LiteralPath $roadOutput -Recurse -File).Count;
    created_utc=[DateTime]::UtcNow.ToString('o')
}
$roadResult | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $roadOutput 'delivery_copy_report.json') -Encoding UTF8
$roadResult | ConvertTo-Json
