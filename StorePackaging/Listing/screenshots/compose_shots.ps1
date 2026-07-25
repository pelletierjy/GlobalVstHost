Add-Type -AssemblyName System.Drawing

$root = 'D:\repos\others\GlobalVSTHost'
$outDir = Join-Path $root 'StorePackaging\Listing\screenshots'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$W = 1920
$H = 1080

# Resolve by glob on the timestamp: the filenames contain a curly apostrophe, and
# hardcoding them breaks when this script is read as ANSI rather than UTF-8.
function Resolve-Shot
{
    param([string]$Stamp)
    $hit = @(Get-ChildItem -Path $root -Filter "*$Stamp*.png" -File)
    if ($hit.Count -ne 1) { throw "expected exactly 1 file matching *$Stamp*.png in $root, found $($hit.Count)" }
    return $hit[0].FullName
}

$src = @{
    Main   = Resolve-Shot '230027'   # 848x682 main window
    Eq     = Resolve-Shot '230114'   # 604x399 EQ editor
    Volume = Resolve-Shot '225939'   # 178x293 master volume
}

# Load, optionally trimming rows from the top (bleed-through from windows behind the
# captured one) and from the bottom (the status bar).
function Load-Cropped
{
    param([string]$Path, [int]$CropTop = 0, [int]$CropBottom = 0)

    $orig = [System.Drawing.Image]::FromFile($Path)
    if ($CropTop -le 0 -and $CropBottom -le 0) { return $orig }

    $h = $orig.Height - $CropTop - $CropBottom
    if ($h -le 0) { throw "crop leaves nothing for $Path" }

    $rect = New-Object System.Drawing.Rectangle 0, $CropTop, $orig.Width, $h
    $cropped = New-Object System.Drawing.Bitmap $rect.Width, $rect.Height
    $g = [System.Drawing.Graphics]::FromImage($cropped)
    $g.DrawImage($orig, (New-Object System.Drawing.Rectangle 0, 0, $rect.Width, $rect.Height), $rect, [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose()
    $orig.Dispose()
    return $cropped
}

# The 848x682 main-window capture's status bar strip starts at y=646 (row luma drops to 14
# and stays; its text occupies 661-674). Cropping to height 646 removes the whole strip
# while keeping the preset panel's bottom border, so the window still ends on a real edge.
# This drops the "CPU: 16.3 % ... HIGH" warning and the device-specific latency figure.
$MAIN_STATUSBAR_ROWS = 682 - 646

$imgMain = Load-Cropped -Path $src.Main   -CropBottom $MAIN_STATUSBAR_ROWS
$imgEq   = Load-Cropped -Path $src.Eq
$imgVol  = Load-Cropped -Path $src.Volume -CropTop 10   # strips desktop text bleed

function New-Canvas
{
    $bmp = New-Object System.Drawing.Bitmap $W, $H
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

    # Vertical gradient in the app's own dark palette so the window sits on it naturally
    $rect = New-Object System.Drawing.Rectangle 0, 0, $W, $H
    $c1 = [System.Drawing.Color]::FromArgb(255, 24, 27, 34)
    $c2 = [System.Drawing.Color]::FromArgb(255, 9, 10, 13)
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $c1, $c2, 90.0
    $g.FillRectangle($brush, $rect)
    $brush.Dispose()

    return @{ Bitmap = $bmp; Graphics = $g }
}

function Draw-WindowWithShadow
{
    param($Graphics, $Image, [int]$X, [int]$Y)

    # Soft shadow: concentric translucent rounded rects behind the window
    for ($i = 14; $i -ge 1; $i--) {
        $alpha = [int](3 + (14 - $i) * 1.2)
        if ($alpha -gt 40) { $alpha = 40 }
        $col = [System.Drawing.Color]::FromArgb($alpha, 0, 0, 0)
        $b = New-Object System.Drawing.SolidBrush $col
        $Graphics.FillRectangle($b, ($X - $i), ($Y - [int]($i / 3) + 6), ($Image.Width + $i * 2), ($Image.Height + $i * 2))
        $b.Dispose()
    }
    $Graphics.DrawImageUnscaled($Image, $X, $Y)
}

function Save-Canvas
{
    param($Canvas, [string]$Name)
    $Canvas.Graphics.Dispose()
    $path = Join-Path $outDir $Name
    $Canvas.Bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $Canvas.Bitmap.Dispose()
    $info = Get-Item $path
    Write-Output ("  {0}  ({1} KB)" -f $Name, [math]::Round($info.Length / 1KB))
}

Write-Output "Writing to $outDir"

# --- 01: main window, centred -------------------------------------------------
$c = New-Canvas
Draw-WindowWithShadow -Graphics $c.Graphics -Image $imgMain `
    -X ([int](($W - $imgMain.Width) / 2)) -Y ([int](($H - $imgMain.Height) / 2))
Save-Canvas -Canvas $c -Name '01-main-window.png'

# --- 02: main window with the EQ editor open over it --------------------------
# Group the two, then centre the group, so it reads as one real desktop arrangement.
$gapX = 300
$groupW = $gapX + $imgEq.Width
$groupH = [Math]::Max($imgMain.Height, 200 + $imgEq.Height)
$originX = [int](($W - $groupW) / 2)
$originY = [int](($H - $groupH) / 2)

$c = New-Canvas
Draw-WindowWithShadow -Graphics $c.Graphics -Image $imgMain -X $originX -Y $originY
Draw-WindowWithShadow -Graphics $c.Graphics -Image $imgEq -X ($originX + $gapX) -Y ($originY + 200)
Save-Canvas -Canvas $c -Name '02-eq-bass-boost.png'

# --- 03: main window with the master volume overlay ---------------------------
$gapX2 = $imgMain.Width - 40
$groupW2 = $gapX2 + $imgVol.Width
$originX2 = [int](($W - $groupW2) / 2)
$originY2 = [int](($H - $imgMain.Height) / 2)

$c = New-Canvas
Draw-WindowWithShadow -Graphics $c.Graphics -Image $imgMain -X $originX2 -Y $originY2
Draw-WindowWithShadow -Graphics $c.Graphics -Image $imgVol `
    -X ($originX2 + $gapX2) -Y ($originY2 + $imgMain.Height - $imgVol.Height - 30)
Save-Canvas -Canvas $c -Name '03-master-volume.png'

$imgMain.Dispose(); $imgEq.Dispose(); $imgVol.Dispose()

Write-Output "`nVerifying dimensions:"
Get-ChildItem (Join-Path $outDir '*.png') | ForEach-Object {
    $i = [System.Drawing.Image]::FromFile($_.FullName)
    $ok = if ($i.Width -ge 1366 -and $i.Height -ge 768) { 'PASS' } else { 'FAIL' }
    Write-Output ("  {0}: {1}x{2}  {3}" -f $_.Name, $i.Width, $i.Height, $ok)
    $i.Dispose()
}
