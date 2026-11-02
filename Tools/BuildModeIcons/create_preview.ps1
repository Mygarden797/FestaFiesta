# Compose actual Unreal-rendered regions for review; production images are unchanged.
Add-Type -AssemblyName System.Drawing
$hkhRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$hkhUser = (Get-Content -LiteralPath (Join-Path $hkhRoot 'Saved/BuildModeIcons/final-user-dir.txt') -Raw).Trim()
$hkhToolbar = [System.Drawing.Bitmap]::new((Join-Path $hkhUser 'Saved/Automation/BuildFeatureGuide/Toolbar_1920x1080.png'))
$hkhCatalog = [System.Drawing.Bitmap]::new((Join-Path $hkhUser 'Saved/Automation/BuildableGrid/BuildCatalog_1920x1080_AllCategories.png'))
$hkhCanvas = [System.Drawing.Bitmap]::new(1320,530)
$hkhGraphics = [System.Drawing.Graphics]::FromImage($hkhCanvas)
$hkhGraphics.Clear([System.Drawing.Color]::FromArgb(31,42,59))
$hkhGraphics.DrawImage($hkhToolbar,[System.Drawing.Rectangle]::new(150,18,1020,56),450,0,1020,56,[System.Drawing.GraphicsUnit]::Pixel)
$hkhGraphics.DrawImage($hkhCatalog,[System.Drawing.Rectangle]::new(10,96,1290,420),12,583,1290,420,[System.Drawing.GraphicsUnit]::Pixel)
$hkhPreview = Join-Path $hkhRoot 'Saved/BuildModeIcons/BuildUI-preview.png'
$hkhCanvas.Save($hkhPreview,[System.Drawing.Imaging.ImageFormat]::Png)
$hkhGraphics.Dispose()
$hkhCanvas.Dispose()
$hkhToolbar.Dispose()
$hkhCatalog.Dispose()
$hkhPreview
