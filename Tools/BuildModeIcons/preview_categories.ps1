Add-Type -AssemblyName System.Drawing
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$sourceDir = Join-Path $taskRoot 'Content/Asset/Icon_HKH'
$previewDir = Join-Path $taskRoot 'Saved/BuildModeIcons'
[System.IO.Directory]::CreateDirectory($previewDir) | Out-Null
$names = @('Booth','Toilet','Info','Rest','Performance','Safety','PhotoSpot','Lighting','Road','Tile')
$canvas = [System.Drawing.Bitmap]::new(1000,300)
$graphics = [System.Drawing.Graphics]::FromImage($canvas)
$graphics.Clear([System.Drawing.Color]::FromArgb(35,43,60))
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$font = [System.Drawing.Font]::new('Segoe UI',10)
$records = @()
for ($index=0; $index -lt $names.Count; $index++) {
    $name = $names[$index]
    $path = Join-Path $sourceDir ('T_HKH_Category_'+$name+'.png')
    $image = [System.Drawing.Bitmap]::new($path)
    $x = $index*100
    $graphics.DrawImage($image,[System.Drawing.Rectangle]::new($x+6,15,88,88))
    $graphics.DrawImage($image,[System.Drawing.Rectangle]::new($x+36,125,28,28))
    $graphics.DrawString($name,$font,[System.Drawing.Brushes]::White,$x+2,180)
    $opaque = 0
    $clear = 0
    $total = 0
    for ($y=0; $y -lt $image.Height; $y+=8) {
        for ($px=0; $px -lt $image.Width; $px+=8) {
            $c = $image.GetPixel($px,$y)
            if ($c.A -gt 250) {$opaque++}
            if ($c.A -eq 0) {$clear++}
            $total++
        }
    }
    $records += [pscustomobject]@{
        Name=$name; Width=$image.Width; Height=$image.Height
        Sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        OpaqueFraction=$opaque/$total; TransparentFraction=$clear/$total
        CornerAlpha=$image.GetPixel(0,0).A
    }
    $image.Dispose()
}
$path = Join-Path $previewDir 'category-qa-dark.png'
$canvas.Save($path,[System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose(); $canvas.Dispose(); $font.Dispose()
$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $previewDir 'category-qa.json') -Encoding utf8
$records | ConvertTo-Json -Depth 5
Write-Output $path
