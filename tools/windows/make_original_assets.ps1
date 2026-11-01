# Generates original replacement artwork for the launcher/UI images in src/assets
# and the Android launcher icons in res/. Everything is drawn procedurally from
# gradients, simple shapes and seeded randomness; no game artwork is used or traced.
# File names and sizes match the images the code already loads.
#
# Windows only (System.Drawing):
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/windows/make_original_assets.ps1
param([string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function C([int]$r, [int]$g, [int]$b, [int]$a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Alpha($color, [int]$a) { [System.Drawing.Color]::FromArgb($a, $color) }
function P([double]$x, [double]$y) { New-Object System.Drawing.PointF ([float]$x), ([float]$y) }

function New-Canvas([int]$w, [int]$h) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.PixelOffsetMode = 'HighQuality'
    $g.CompositingQuality = 'HighQuality'
    @{ Bitmap = $bmp; G = $g; W = $w; H = $h }
}

function Save-Canvas($c, [string]$rel) {
    $path = Join-Path $Root $rel
    New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null
    $c.G.Dispose()
    $c.Bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $c.Bitmap.Dispose()
    Write-Host "  $rel"
}

function Fill-Sky($c, $top, $bottom) {
    $rect = New-Object System.Drawing.RectangleF 0, -1, $c.W, ($c.H + 2)
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $top, $bottom, 90.0
    $c.G.FillRectangle($brush, $rect)
    $brush.Dispose()
}

function Add-Glow($c, [double]$x, [double]$y, [double]$r, $color) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddEllipse([float]($x - $r), [float]($y - $r), [float](2 * $r), [float](2 * $r))
    $brush = New-Object System.Drawing.Drawing2D.PathGradientBrush $path
    $brush.CenterColor = $color
    $brush.SurroundColors = [System.Drawing.Color[]]@((Alpha $color 0))
    $c.G.FillPath($brush, $path)
    $brush.Dispose(); $path.Dispose()
}

function Add-Disc($c, [double]$x, [double]$y, [double]$r, $color) {
    $b = New-Object System.Drawing.SolidBrush $color
    $c.G.FillEllipse($b, [float]($x - $r), [float]($y - $r), [float](2 * $r), [float](2 * $r))
    $b.Dispose()
}

function Add-Stars($c, $rng, [int]$count, [double]$maxY, $color) {
    for ($i = 0; $i -lt $count; $i++) {
        $r = 0.5 + [Math]::Pow($rng.NextDouble(), 3) * 1.8
        Add-Disc $c ($rng.NextDouble() * $c.W) ($rng.NextDouble() * $maxY) $r (Alpha $color ([int](70 + $rng.NextDouble() * 185)))
    }
}

function Add-Fog($c, [double]$y, [double]$h, $color) {
    $rect = New-Object System.Drawing.RectangleF 0, ([float]$y), $c.W, ([float]$h)
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $color, $color, 90.0
    $blend = New-Object System.Drawing.Drawing2D.ColorBlend 3
    $blend.Colors = [System.Drawing.Color[]]@((Alpha $color 0), $color, (Alpha $color 0))
    $blend.Positions = [single[]]@(0, 0.5, 1)
    $brush.InterpolationColors = $blend
    $brush.WrapMode = 'TileFlipXY'  # avoids GDI+'s hard line at the gradient edges
    $c.G.FillRectangle($brush, $rect)
    $brush.Dispose()
}

# Skyline from three sine waves (optionally roughened), filled down to the bottom.
# Returns the ridge height function as an array indexed by x.
function Add-Ridge($c, $rng, [double]$base, [double]$amp, $color, [switch]$Jagged) {
    $ph = 0..2 | ForEach-Object { $rng.NextDouble() * 6.2832 }
    $fr = @((1 + $rng.NextDouble()), (2.5 + 2 * $rng.NextDouble()), (6 + 5 * $rng.NextDouble()))
    $heights = New-Object double[] ($c.W + 1)
    $pts = New-Object System.Collections.Generic.List[System.Drawing.PointF]
    $pts.Add((P -2 ($c.H + 2)))
    for ($x = 0; $x -le $c.W; $x++) {
        $t = $x / $c.W * 6.2832
        $y = $base - $amp * (0.55 * [Math]::Sin($fr[0] * $t + $ph[0]) + 0.3 * [Math]::Sin($fr[1] * $t + $ph[1]) + 0.15 * [Math]::Sin($fr[2] * $t + $ph[2]))
        if ($Jagged -and $x % 12 -eq 0) { $y += ($rng.NextDouble() - 0.5) * $amp * 0.4 }
        $heights[$x] = $y
        if ($x % 4 -eq 0 -or $x -eq $c.W) { $pts.Add((P $x $y)) }
    }
    $pts.Add((P ($c.W + 2) ($c.H + 2)))
    $b = New-Object System.Drawing.SolidBrush $color
    $c.G.FillPolygon($b, $pts.ToArray())
    $b.Dispose()
    return ,$heights
}

function Add-Blob($c, $rng, [double]$cx, [double]$cy, [double]$r, $color, [int]$parts = 9) {
    for ($i = 0; $i -lt $parts; $i++) {
        $a = $rng.NextDouble() * 6.2832; $d = $rng.NextDouble() * $r * 0.55
        Add-Disc $c ($cx + [Math]::Cos($a) * $d) ($cy + [Math]::Sin($a) * $d * 0.8) ($r * (0.45 + 0.25 * $rng.NextDouble())) $color
    }
}

function Add-Trunk($c, [double]$x, [double]$top, [double]$bottom, [double]$w, $color) {
    $b = New-Object System.Drawing.SolidBrush $color
    $c.G.FillPolygon($b, [System.Drawing.PointF[]]@((P ($x - $w / 2) $bottom), (P ($x + $w / 2) $bottom), (P ($x + $w * 0.3) $top), (P ($x - $w * 0.3) $top)))
    $b.Dispose()
}

function Add-Branches($c, $rng, [double]$x, [double]$y, [double]$len, [double]$angle, [double]$width, $color, [int]$depth) {
    if ($depth -le 0 -or $len -lt 4) { return }
    $x2 = $x + [Math]::Cos($angle) * $len; $y2 = $y + [Math]::Sin($angle) * $len
    $pen = New-Object System.Drawing.Pen $color, ([float]$width)
    $pen.StartCap = 'Round'; $pen.EndCap = 'Round'
    $c.G.DrawLine($pen, [float]$x, [float]$y, [float]$x2, [float]$y2)
    $pen.Dispose()
    foreach ($turn in @(-0.45, 0.4)) {
        Add-Branches $c $rng $x2 $y2 ($len * (0.62 + 0.15 * $rng.NextDouble())) ($angle + $turn + ($rng.NextDouble() - 0.5) * 0.3) ($width * 0.62) $color ($depth - 1)
    }
}

function New-RoundedRect([double]$x, [double]$y, [double]$w, [double]$h, [double]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = [float](2 * $r)
    $p.AddArc([float]$x, [float]$y, $d, $d, 180, 90)
    $p.AddArc([float]($x + $w - 2 * $r), [float]$y, $d, $d, 270, 90)
    $p.AddArc([float]($x + $w - 2 * $r), [float]($y + $h - 2 * $r), $d, $d, 0, 90)
    $p.AddArc([float]$x, [float]($y + $h - 2 * $r), $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

# A plain, generic sword pointing up, centered on (0,0); $L is the total length.
function Draw-Sword($g, [double]$L, $blade, $edge, $guard, $grip) {
    $w = $L * 0.11; $tip = -$L * 0.5; $base = $L * 0.17
    $b = New-Object System.Drawing.SolidBrush $blade
    $g.FillPolygon($b, [System.Drawing.PointF[]]@((P 0 $tip), (P ($w / 2) ($tip + $w * 1.4)), (P ($w / 2) $base), (P (-$w / 2) $base), (P (-$w / 2) ($tip + $w * 1.4))))
    $b.Dispose()
    $b = New-Object System.Drawing.SolidBrush $edge
    $g.FillPolygon($b, [System.Drawing.PointF[]]@((P 0 $tip), (P ($w / 2) ($tip + $w * 1.4)), (P ($w / 2) $base), (P 0 $base)))
    $b.Dispose()
    $b = New-Object System.Drawing.SolidBrush $guard
    $gp = New-RoundedRect (-$L * 0.17) $base ($L * 0.34) ($L * 0.055) ($L * 0.02)
    $g.FillPath($b, $gp); $gp.Dispose()
    $g.FillEllipse($b, [float](-$L * 0.045), [float]($L * 0.395), [float]($L * 0.09), [float]($L * 0.09))
    $b.Dispose()
    $b = New-Object System.Drawing.SolidBrush $grip
    $g.FillRectangle($b, [float](-$w * 0.32), [float]($base + $L * 0.055), [float]($w * 0.64), [float]($L * 0.18))
    $b.Dispose()
}

# Rounded square, soft glow and a diagonal sword (same motif as the starter icon).
function New-IconMaster($top, $bottom, $glow) {
    $c = New-Canvas 512 512
    $bg = New-RoundedRect 12 12 488 488 104
    $rect = New-Object System.Drawing.RectangleF 0, 0, 512, 512
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $top, $bottom, 90.0
    $c.G.FillPath($brush, $bg); $brush.Dispose()
    $c.G.SetClip($bg)
    Add-Glow $c 256 236 230 (Alpha $glow 120)
    $c.G.ResetClip(); $bg.Dispose()
    $c.G.TranslateTransform(270, 270); $c.G.RotateTransform(45)
    Draw-Sword $c.G 400 (C 0 0 0 70) (C 0 0 0 0) (C 0 0 0 70) (C 0 0 0 70)
    $c.G.ResetTransform()
    $c.G.TranslateTransform(256, 256); $c.G.RotateTransform(45)
    Draw-Sword $c.G 400 (C 214 230 242) (C 246 251 255) (C 222 170 64) (C 92 58 40)
    $c.G.ResetTransform()
    return $c
}

function Save-Icon($master, [int]$size, [string]$rel) {
    $c = New-Canvas $size $size
    $c.G.DrawImage($master.Bitmap, 0, 0, $size, $size)
    Save-Canvas $c $rel
}

Write-Host 'Icons'
$icons = @(
    @{ Top = (C 40 78 128); Bottom = (C 16 32 62); Glow = (C 120 190 255); Files = @(
        @(144, 'src/assets/icons/swordigo_default.png'), @(144, 'src/assets/icon_gnome.png'), @(128, 'src/assets/icon_x11.png'),
        @(48, 'res/drawable-mdpi/ic_launcher.png'), @(72, 'res/drawable-hdpi/ic_launcher.png'),
        @(96, 'res/drawable-xhdpi/ic_launcher.png'), @(144, 'res/drawable-xxhdpi/ic_launcher.png')) },
    @{ Top = (C 138 44 44); Bottom = (C 52 16 24); Glow = (C 255 150 90); Files = @(,@(1024, 'src/assets/icons/rl_swordigo_default.png')) },
    @{ Top = (C 44 120 76); Bottom = (C 16 50 36); Glow = (C 150 255 170); Files = @(,@(512, 'src/assets/icons/swmini_default.png')) }
)
foreach ($icon in $icons) {
    $master = New-IconMaster $icon.Top $icon.Bottom $icon.Glow
    foreach ($f in $icon.Files) { Save-Icon $master $f[0] $f[1] }
    $master.G.Dispose(); $master.Bitmap.Dispose()
}

Write-Host 'Backgrounds'
# Night sky with hills (map editor default, 840x500).
$rng = New-Object System.Random 101
$c = New-Canvas 840 500
Fill-Sky $c (C 10 32 58) (C 26 92 118)
Add-Stars $c $rng 140 330 (C 230 245 255)
Add-Glow $c 640 120 110 (C 200 230 255 90)
Add-Disc $c 640 120 26 (C 236 246 255)
$null = Add-Ridge $c $rng 360 60 (C 14 44 64)
$null = Add-Ridge $c $rng 420 40 (C 8 26 40)
Save-Canvas $c 'src/assets/bg0.png'

# Daytime hills (960x544).
$rng = New-Object System.Random 202
$c = New-Canvas 960 544
Fill-Sky $c (C 70 150 210) (C 196 228 240)
Add-Glow $c 180 110 160 (C 255 250 220 150)
foreach ($i in 1..6) { Add-Blob $c $rng (100 + $rng.NextDouble() * 760) (60 + $rng.NextDouble() * 160) (40 + $rng.NextDouble() * 30) (C 255 255 255 70) 7 }
$null = Add-Ridge $c $rng 330 70 (C 120 160 190) -Jagged
$null = Add-Ridge $c $rng 410 45 (C 96 160 92)
$null = Add-Ridge $c $rng 470 30 (C 64 124 66)
Save-Canvas $c 'src/assets/pic0.png'

# Cave: stalactites and stalagmites around a cold light (1024x1024).
$rng = New-Object System.Random 303
$c = New-Canvas 1024 1024
Fill-Sky $c (C 22 18 16) (C 10 9 9)
Add-Glow $c 512 560 420 (C 80 120 150 110)
foreach ($layer in @(@{ Col = (C 46 38 34); Len = 380; Step = 70 }, @{ Col = (C 20 16 15); Len = 300; Step = 55 })) {
    $b = New-Object System.Drawing.SolidBrush $layer.Col
    $x = -20
    while ($x -lt 1044) {
        $w = 30 + $rng.NextDouble() * $layer.Step; $len = 60 + $rng.NextDouble() * $layer.Len
        $c.G.FillPolygon($b, [System.Drawing.PointF[]]@((P $x -2), (P ($x + $w) -2), (P ($x + $w * (0.3 + 0.4 * $rng.NextDouble())) $len)))
        $len = 40 + $rng.NextDouble() * $layer.Len * 0.7
        $c.G.FillPolygon($b, [System.Drawing.PointF[]]@((P $x 1026), (P ($x + $w) 1026), (P ($x + $w * (0.3 + 0.4 * $rng.NextDouble())) (1024 - $len))))
        $x += $w * (0.6 + 0.5 * $rng.NextDouble())
    }
    $b.Dispose()
}
Add-Fog $c 640 220 (C 90 120 140 40)
Save-Canvas $c 'src/assets/caves_bg.png'

# Misty forest (1024x512).
$rng = New-Object System.Random 404
$c = New-Canvas 1024 512
Fill-Sky $c (C 30 52 40) (C 8 16 12)
foreach ($depth in @(@{ Col = (C 40 62 48); N = 14; W = 18 }, @{ Col = (C 26 40 30); N = 10; W = 30 }, @{ Col = (C 14 22 16); N = 6; W = 46 })) {
    for ($i = 0; $i -lt $depth.N; $i++) {
        $x = $rng.NextDouble() * 1024
        Add-Trunk $c $x -10 520 ($depth.W * (0.7 + 0.6 * $rng.NextDouble())) $depth.Col
        Add-Blob $c $rng ($x + ($rng.NextDouble() - 0.5) * 60) (40 + $rng.NextDouble() * 120) (60 + $rng.NextDouble() * 40) $depth.Col 8
    }
    Add-Fog $c 220 200 (C 150 180 160 35)
}
$null = Add-Ridge $c $rng 470 25 (C 10 18 12)
Save-Canvas $c 'src/assets/forest_bg.png'

# Grasslands under a bright sky (1024x1024).
$rng = New-Object System.Random 505
$c = New-Canvas 1024 1024
Fill-Sky $c (C 84 160 220) (C 214 236 236)
Add-Glow $c 780 220 260 (C 255 246 210 160)
foreach ($i in 1..7) { Add-Blob $c $rng ($rng.NextDouble() * 1024) (100 + $rng.NextDouble() * 280) (50 + $rng.NextDouble() * 40) (C 255 255 255 80) 8 }
$null = Add-Ridge $c $rng 600 110 (C 130 166 196) -Jagged
$null = Add-Ridge $c $rng 700 70 (C 120 176 98)
$null = Add-Ridge $c $rng 800 60 (C 88 150 72)
$null = Add-Ridge $c $rng 900 50 (C 60 118 54)
Save-Canvas $c 'src/assets/grasslands_bg.png'

# Graveyard at dusk: moon, hills, simple headstones and a bare tree (1024x1024).
$rng = New-Object System.Random 606
$c = New-Canvas 1024 1024
Fill-Sky $c (C 38 30 62) (C 120 92 120)
Add-Stars $c $rng 90 450 (C 240 230 255)
Add-Glow $c 300 260 220 (C 230 220 255 90)
Add-Disc $c 300 260 54 (C 238 234 250)
$null = Add-Ridge $c $rng 690 80 (C 62 50 82)
$stone = New-Object System.Drawing.SolidBrush (C 34 28 44)
foreach ($i in 0..8) {
    $x = 60 + $i * 112 + ($rng.NextDouble() - 0.5) * 40; $h = 50 + $rng.NextDouble() * 40; $w = 30 + $rng.NextDouble() * 18
    if ($i % 3 -eq 1) {
        $c.G.FillRectangle($stone, [float]($x - 5), [float](850 - $h * 1.3), 10, [float]($h * 1.3 + 20))
        $c.G.FillRectangle($stone, [float]($x - $w / 2), [float](850 - $h), [float]$w, 10)
    } else {
        $p = New-RoundedRect ($x - $w / 2) (850 - $h) $w ($h + 40) ($w / 2 - 1)
        $c.G.FillPath($stone, $p); $p.Dispose()
    }
}
$stone.Dispose()
Add-Branches $c $rng 860 880 170 -1.62 22 (C 28 22 36) 6
$null = Add-Ridge $c $rng 880 30 (C 28 22 36)
Add-Fog $c 760 180 (C 200 190 220 45)
Save-Canvas $c 'src/assets/graveyard_bg.png'

# Grove: round trees and fireflies (1024x1024).
$rng = New-Object System.Random 707
$c = New-Canvas 1024 1024
Fill-Sky $c (C 24 86 92) (C 10 34 36)
foreach ($depth in @(@{ Col = (C 30 96 92); N = 7; R = 90 }, @{ Col = (C 20 66 62); N = 6; R = 120 }, @{ Col = (C 10 38 36); N = 4; R = 150 })) {
    for ($i = 0; $i -lt $depth.N; $i++) {
        $x = $rng.NextDouble() * 1024; $top = 380 + $rng.NextDouble() * 220
        Add-Trunk $c $x $top 1030 ($depth.R * 0.22) $depth.Col
        Add-Blob $c $rng $x $top $depth.R $depth.Col 10
    }
    Add-Fog $c 640 260 (C 160 220 210 30)
}
for ($i = 0; $i -lt 40; $i++) {
    $x = $rng.NextDouble() * 1024; $y = 300 + $rng.NextDouble() * 650
    Add-Glow $c $x $y 12 (C 255 240 140 150)
    Add-Disc $c $x $y 2 (C 255 250 200)
}
Save-Canvas $c 'src/assets/grove_bg.png'

# Launcher background: dark teal with soft clouds and small stars (1024x1024).
$rng = New-Object System.Random 808
$c = New-Canvas 1024 1024
Fill-Sky $c (C 14 44 52) (C 8 24 30)
for ($i = 0; $i -lt 14; $i++) {
    Add-Glow $c ($rng.NextDouble() * 1024) ($rng.NextDouble() * 1024) (160 + $rng.NextDouble() * 220) (Alpha ($(if ($i % 2) { C 4 14 18 } else { C 40 90 100 })) 60)
}
Add-Stars $c $rng 160 1024 (C 220 240 245)
Save-Canvas $c 'src/assets/launcher_bg.png'

# Splash fallback without any logo text (280x158).
$c = New-Canvas 280 158
Fill-Sky $c (C 22 58 92) (C 10 26 44)
Add-Glow $c 140 79 90 (C 120 190 255 110)
$c.G.TranslateTransform(140, 79); $c.G.RotateTransform(45)
Draw-Sword $c.G 120 (C 214 230 242) (C 246 251 255) (C 222 170 64) (C 92 58 40)
$c.G.ResetTransform()
Save-Canvas $c 'src/assets/startup.png'
Write-Host 'Done.'
