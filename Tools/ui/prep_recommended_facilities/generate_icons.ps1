# 프로젝트 전용 선형 아이콘 3종. 외부 이미지 없이 도형으로 제작한다.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$iconRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../SourceArt/UI/PrepRecommended'))
New-Item -ItemType Directory -Force -Path $iconRoot | Out-Null
foreach ($kind in @('Lighting','FirstAid','FireSafety')) {
    $bitmap = [Drawing.Bitmap]::new(256,256)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.Clear([Drawing.Color]::Transparent)
    $pen = [Drawing.Pen]::new([Drawing.Color]::White,12)
    $pen.StartCap = $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    if ($kind -eq 'Lighting') {
        $points = [Drawing.PointF[]]@([Drawing.PointF]::new(46,100),[Drawing.PointF]::new(128,42),[Drawing.PointF]::new(210,100),[Drawing.PointF]::new(46,100))
        $graphics.DrawLines($pen,$points)
        $graphics.DrawLine($pen,128,100,128,220)
        $graphics.DrawLine($pen,92,222,164,222)
        $graphics.DrawLine($pen,104,114,104,135)
        $graphics.DrawLine($pen,152,114,152,135)
        $graphics.DrawLine($pen,128,26,128,42)
    } elseif ($kind -eq 'FirstAid') {
        $coords = @(98,36,158,36,158,98,220,98,220,158,158,158,158,220,98,220,98,158,36,158,36,98,98,98,98,36)
        $points = for ($i=0; $i -lt $coords.Count; $i+=2) { [Drawing.PointF]::new($coords[$i],$coords[$i+1]) }
        $graphics.DrawLines($pen,[Drawing.PointF[]]$points)
    } else {
        $graphics.DrawArc($pen,82,66,84,64,180,180)
        $graphics.DrawLine($pen,82,98,82,224)
        $graphics.DrawLine($pen,166,98,166,224)
        $graphics.DrawLine($pen,82,224,166,224)
        $graphics.DrawLine($pen,85,145,162,145)
        $graphics.DrawLine($pen,85,181,162,181)
        $graphics.DrawLine($pen,121,64,121,35)
        $graphics.DrawLine($pen,100,35,158,35)
        $graphics.DrawLine($pen,152,23,152,50)
        $graphics.DrawBezier($pen,108,48,53,38,46,65,46,124)
        $graphics.DrawLine($pen,46,124,36,138)
    }
    $bitmap.Save((Join-Path $iconRoot ($kind + '.png')),[Drawing.Imaging.ImageFormat]::Png)
    $pen.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
}
Write-Output 'PREP_RECOMMENDED_ICONS_CREATED'
