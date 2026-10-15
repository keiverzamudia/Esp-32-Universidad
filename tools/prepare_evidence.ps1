<#
.SYNOPSIS
  TRACE-X - preparador de evidencias para la flash del ESP32.
.DESCRIPTION
  Lee tools/scenarios.json + evidence_source/ (capturas reales opcionales),
  genera JPEGs optimizados (full + thumb) y evidence_build/manifest.json.
  Si no hay captura real para un escenario, crea una imagen ilustrativa
  marcada explicitamente como DEMOSTRACION EDUCATIVA.
.NOTES
  Requiere: Windows PowerShell 5.1+ con System.Drawing (incluido en Windows).
  Uso:  powershell -ExecutionPolicy Bypass -File tools\prepare_evidence.ps1
#>
param(
    [string]$ProjectRoot = (Split-Path $PSScriptRoot -Parent),
    [int]$FullWidth = 640,
    [int]$ThumbWidth = 200,
    [int]$FullMaxKB = 50,
    [int]$ThumbMaxKB = 10,
    [int]$TotalMaxKB = 350
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$srcDir  = Join-Path $ProjectRoot 'evidence_source'
$buildDir = Join-Path $ProjectRoot 'evidence_build'
$scenPath = Join-Path $ProjectRoot 'tools\scenarios.json'
$provPath = Join-Path $srcDir 'provenance.json'

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Get-ChildItem $buildDir -File -ErrorAction SilentlyContinue | Remove-Item -Force

$scenarios = (Get-Content $scenPath -Raw -Encoding UTF8 | ConvertFrom-Json).scenarios
$provenance = @{}
if (Test-Path $provPath) {
    $prov = Get-Content $provPath -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($p in $prov.imports) { $provenance[$p.id] = $p }
}

function New-JpegEncoder([long]$quality) {
    $codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() |
        Where-Object { $_.MimeType -eq 'image/jpeg' }
    $params = New-Object System.Drawing.Imaging.EncoderParameters(1)
    $params.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter(
        [System.Drawing.Imaging.Encoder]::Quality, $quality)
    return @($codec, $params)
}

function Save-Jpeg($bmp, [string]$path, [long]$quality) {
    $enc = New-JpegEncoder $quality
    $bmp.Save($path, $enc[0], $enc[1])
}

function Resize-Image([System.Drawing.Image]$img, [int]$targetW) {
    $ratio = $targetW / $img.Width
    $h = [Math]::Max(1, [int][Math]::Round($img.Height * $ratio))
    $bmp = New-Object System.Drawing.Bitmap($targetW, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.DrawImage($img, 0, 0, $targetW, $h)
    $g.Dispose()
    return $bmp
}

function New-DemoScreenshot($scen) {
    $w = 640; $h = 400
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::ClearTypeGridFit

    $bg    = [System.Drawing.Color]::FromArgb(255, 12, 16, 24)
    $bar   = [System.Drawing.Color]::FromArgb(255, 22, 30, 42)
    $page  = [System.Drawing.Color]::FromArgb(255, 245, 246, 250)
    $ink   = [System.Drawing.Color]::FromArgb(255, 30, 34, 42)
    $mut   = [System.Drawing.Color]::FromArgb(255, 120, 126, 140)
    $cy    = [System.Drawing.Color]::FromArgb(255, 46, 230, 255)
    $red   = [System.Drawing.Color]::FromArgb(255, 200, 40, 55)
    $green = [System.Drawing.Color]::FromArgb(255, 20, 160, 90)
    $amber = [System.Drawing.Color]::FromArgb(255, 200, 140, 20)

    $g.Clear($bg)
    $g.FillRectangle((New-Object System.Drawing.SolidBrush($bar)), 0, 0, $w, 34)
    foreach ($i in 0..2) {
        $c = @($red, $amber, $green)[$i]
        $g.FillEllipse((New-Object System.Drawing.SolidBrush($c)), 12 + $i * 18, 12, 10, 10)
    }
    $fontUi   = New-Object System.Drawing.Font('Consolas', 9)
    $fontSm   = New-Object System.Drawing.Font('Consolas', 7.5)
    $fontBold = New-Object System.Drawing.Font('Consolas', 11, [System.Drawing.FontStyle]::Bold)
    $brushMut = New-Object System.Drawing.SolidBrush($mut)
    $brushInk = New-Object System.Drawing.SolidBrush($ink)
    $g.DrawString('https://app.demo.test/escenario', $fontUi, $brushMut, 72, 10)

    $g.FillRectangle((New-Object System.Drawing.SolidBrush($page)), 12, 46, $w - 24, $h - 106)
    $isPass = $scen.status -eq 'PASS'
    $accent = if ($isPass) { $green } elseif ($scen.status -eq 'FAIL') { $red } else { $cy }
    $g.FillRectangle((New-Object System.Drawing.SolidBrush($accent)), 12, 46, 5, $h - 106)

    $g.DrawString($scen.name, $fontBold, $brushInk, 30, 62)
    $g.DrawString(('expect: ' + $scen.expected.Substring(0, [Math]::Min(72, $scen.expected.Length))), $fontSm, $brushMut, 30, 92)

    $boxColor = if ($isPass) { $green } else { $red }
    $boxBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(40, $boxColor.R, $boxColor.G, $boxColor.B))
    $g.FillRectangle($boxBrush, 30, 122, $w - 60, 96)
    $g.DrawRectangle((New-Object System.Drawing.Pen($boxColor, 1.5)), 30, 122, $w - 60, 96)
    $boxInk = New-Object System.Drawing.SolidBrush($boxColor)
    $label = if ($isPass) { '✓ PASS  expect(received) == expected' } else { '✗ FAIL  ' + $scen.status + ' assertion' }
    $g.DrawString($label, $fontUi, $boxInk, 44, 136)
    $obsLine = 'observed: ' + $scen.observed
    $g.DrawString($obsLine.Substring(0, [Math]::Min(86, $obsLine.Length)), $fontSm, $brushInk, 44, 160)
    $g.DrawString('-' * 68, $fontSm, $brushMut, 44, 184)

    $y = 234
    $n = 0
    foreach ($st in $scen.steps) {
        if ($n -ge 5) { break }
        $g.DrawString(('  {0}. {1}' -f ($n + 1), $st.Substring(0, [Math]::Min(74, $st.Length))), $fontSm, $brushInk, 30, $y)
        $y += 18; $n++
    }

    $g.DrawString('Playwright 1.4x · Chromium · traza + capturas en test-results/', $fontSm, $brushMut, 30, $h - 52)

    $banner = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 60, 44, 8))
    $g.FillRectangle($banner, 0, $h - 26, $w, 26)
    $g.DrawString('DEMOSTRACIÓN EDUCATIVA — imagen ilustrativa, no es una captura de un test real',
        $fontSm, (New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 255, 200, 90))), 18, $h - 20)

    $g.Dispose()
    return $bmp
}

function Get-OrCreateFull([string]$id, [bool]$authentic, [string]$realPath, $scen) {
    if ($authentic -and $realPath -and (Test-Path $realPath)) {
        $img = [System.Drawing.Image]::FromFile($realPath)
        $bmp = Resize-Image $img $FullWidth
        $img.Dispose()
        return $bmp
    }
    return New-DemoScreenshot $scen
}

$items = @()
$totalBytes = 0
$log = @()

foreach ($scen in $scenarios) {
    $id = $scen.id
    $authentic = $false
    $realFile = $null
    $sourceDesc = 'Ilustración generada por tools/prepare_evidence.ps1'
    $note = 'DEMOSTRACIÓN EDUCATIVA: imagen ilustrativa, no corresponde a una ejecución real.'

    if ($provenance.ContainsKey($id)) {
        $entry = $provenance[$id]
        $cand = Join-Path $srcDir $entry.file
        if (Test-Path $cand) {
            $authentic = $true
            $realFile = $cand
            $sourceDesc = $entry.source
            $note = $entry.note
        } else {
            $log += "WARN ${id}: provenance apunta a '$($entry.file)' pero no existe; se usa ilustracion."
        }
    }

    $fullBmp = Get-OrCreateFull $id $authentic $realFile $scen
    $fullPath = Join-Path $buildDir "$($id)_full.jpg"
    Save-Jpeg $fullBmp $fullPath 72
    $fullBmp.Dispose()

    $thumbSrc = [System.Drawing.Image]::FromFile($fullPath)
    $thumbBmp = Resize-Image $thumbSrc $ThumbWidth
    $thumbSrc.Dispose()
    $thumbPath = Join-Path $buildDir "$($id)_thumb.jpg"
    Save-Jpeg $thumbBmp $thumbPath 62
    $thumbBmp.Dispose()

    $fullKB  = [Math]::Round((Get-Item $fullPath).Length / 1KB, 1)
    $thumbKB = [Math]::Round((Get-Item $thumbPath).Length / 1KB, 1)
    if ($fullKB -gt $FullMaxKB)  { throw "$id`_full.jpg = $fullKB KB excede limite $FullMaxKB KB" }
    if ($thumbKB -gt $ThumbMaxKB) { throw "$id`_thumb.jpg = $thumbKB KB excede limite $ThumbMaxKB KB" }
    $totalBytes += (Get-Item $fullPath).Length + (Get-Item $thumbPath).Length
    $log += ("{0} {1} full={2} KB thumb={3} KB authentic={4}" -f $id, $scen.status, $fullKB, $thumbKB, $authentic)

    $items += [ordered]@{
        id           = $id
        name         = $scen.name
        status       = $scen.status
        authentic    = $authentic
        source       = $sourceDesc
        note         = $note
        explanation  = $scen.explanation
        expected     = $scen.expected
        observed     = $scen.observed
        steps        = $scen.steps
        related      = $scen.related
    }
}

$totalKB = [Math]::Round($totalBytes / 1KB, 1)
if ($totalKB -gt $TotalMaxKB) { throw "Total imagenes $totalKB KB excede presupuesto $TotalMaxKB KB" }

$manifest = [ordered]@{
    schema     = 1
    project    = 'TRACE-X'
    generated  = (Get-Date).ToString('yyyy-MM-ddTHH:mm:ssK')
    note       = 'Evidencias preprocesadas en el equipo de desarrollo. El ESP32 las sirve tal cual; no genera trazas en vivo.'
    total_bytes = $totalBytes
    items      = $items
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $buildDir 'manifest.json') -Encoding UTF8

Write-Host ''
Write-Host '=== TRACE-X evidence budget ===' -ForegroundColor Cyan
$log | ForEach-Object { Write-Host "  $_" }
Write-Host ("  TOTAL imagenes: {0} KB (limite {1} KB)" -f $totalKB, $TotalMaxKB) -ForegroundColor Green
Write-Host ("  Manifest: {0} bytes" -f (Get-Item (Join-Path $buildDir 'manifest.json')).Length)
Write-Host ''
Write-Host 'Para usar capturas REALES: copialas a evidence_source\' -ForegroundColor Yellow
Write-Host 'y registralas en evidence_source\provenance.json con su origen.' -ForegroundColor Yellow
