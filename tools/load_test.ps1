<#
.SYNOPSIS
  TRACE-X — Prueba de carga sintética sobre el servidor HTTP del ESP32.
  Compatible con Windows PowerShell 5.1 (sin dependencias externas).
.DESCRIPTION
  Lanza N trabajadores concurrentes en un runspace pool que solicitan las
  rutas reales del firmware (estado, manifiesto, imágenes) durante T
  segundos y reporta latencias p50/p95/max, errores, throughput y PASS/FAIL.
.USAGE
  powershell -ExecutionPolicy Bypass -File tools\load_test.ps1 `
      -BaseUrl http://192.168.4.1 -Clients 4 -DurationSec 60
#>
param(
    [string]$BaseUrl = "http://192.168.4.1",
    [int]$Clients = 4,
    [int]$DurationSec = 60,
    [int]$PollMs = 1000
)

$ErrorActionPreference = "Stop"

# Rutas reales del firmware con pesos relativos (poll 50, imgs 30, resto 20)
$routes = @(
    @{ w = 50; path = "/api/state?since=0" },
    @{ w = 25; path = "/" },
    @{ w = 15; path = "/app.css" },
    @{ w = 10; path = "/app.js" },
    @{ w = 15; path = "/api/gallery" },
    @{ w = 20; path = "/img/ev01_full.jpg" },
    @{ w = 30; path = "/img/ev01_thumb.jpg" }
)
$weighted = @()
foreach ($r in $routes) { for ($i = 0; $i -lt $r.w; $i++) { $weighted += $r.path } }

$workerScript = {
    param([string]$base, [string[]]$paths, [int]$pollMs, [datetime]$deadline, [int]$seed)
    $rand = [Random]::new($seed)
    $ok = 0; $fail = 0; $bytes = 0
    $lat = New-Object System.Collections.Generic.List[double]
    $errs = @{}
    while ((Get-Date) -lt $deadline) {
        $p = $paths[$rand.Next($paths.Count)]
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        try {
            $r = Invoke-WebRequest -Uri ($base + $p) -TimeoutSec 10 -UseBasicParsing
            $sw.Stop()
            $len = if ($null -ne $r.Content -and $r.Content -is [byte[]]) { $r.Content.Length } else { $r.RawContentLength }
            $ok++; $bytes += $len; $lat.Add($sw.Elapsed.TotalMilliseconds)
        } catch {
            $sw.Stop()
            $code = "err"
            if ($_.Exception.Response) { $code = [int]$_.Exception.Response.StatusCode }
            $fail++
            $errs["$code"] = 1 + [int]$errs["$code"]
        }
        if ($pollMs -gt 0) { Start-Sleep -Milliseconds $pollMs }
    }
    [pscustomobject]@{ ok = $ok; fail = $fail; bytes = $bytes; lat = $lat; errs = $errs }
}

$deadline = (Get-Date).AddSeconds($DurationSec)
$pool = [runspacefactory]::CreateRunspacePool(1, $Clients)
$pool.Open()
$workers = @()
for ($i = 1; $i -le $Clients; $i++) {
    $ps = [powershell]::Create()
    $ps.RunspacePool = $pool
    [void]$ps.AddScript($workerScript).AddArgument($BaseUrl).AddArgument($weighted).AddArgument($PollMs).AddArgument($deadline).AddArgument([Environment]::TickCount + $i)
    $workers += [pscustomobject]@{ PS = $ps; Handle = $ps.BeginInvoke() }
}

# Barra de progreso simple cada 20% mientras esperan
$allLat = New-Object System.Collections.Generic.List[double]
$tOk = 0; $tFail = 0; $tBytes = 0; $tErrs = @{}
foreach ($w in $workers) {
    $res = $w.PS.EndInvoke($w.Handle)
    $w.PS.Dispose()
    foreach ($r in $res) {
        $tOk += $r.ok; $tFail += $r.fail; $tBytes += $r.bytes
        foreach ($l in $r.lat) { $allLat.Add($l) }
        foreach ($k in $r.errs.Keys) { $tErrs[$k] = [int]$tErrs[$k] + [int]$r.errs[$k] }
    }
}
$pool.Close()

$lat = $allLat | Sort-Object
$n = $lat.Count
$pctile = { param($q) if ($n -eq 0) { return 0 } return [Math]::Round($lat[[Math]::Min($n - 1, [int][Math]::Floor($q * $n))], 1) }
$p50 = & $pctile 0.50; $p95 = & $pctile 0.95; $pmax = if ($n -gt 0) { $lat[$n - 1] } else { 0 }
$total = $tOk + $tFail
$failRate = if ($total -gt 0) { $tFail / $total } else { 1 }
$pass = ($total -gt 0) -and ($failRate -lt 0.01) -and ($p95 -lt 500) -and ($tErrs.Count -eq 0 -or (($tErrs.Keys | Where-Object { $_ -ne '503' } | Measure-Object).Count -eq 0))

$report = [ordered]@{
    base_url        = $BaseUrl
    clients         = $Clients
    duration_sec    = $DurationSec
    requests        = $total
    ok              = $tOk
    fail            = $tFail
    throughput_rps  = [Math]::Round($tOk / [Math]::Max(1, $DurationSec), 2)
    bytes_total     = $tBytes
    latency_ms      = [ordered]@{ p50 = $p50; p95 = $p95; max = [Math]::Round($pmax, 1) }
    errors          = $tErrs
    pass_criteria   = "fail<1% AND p95<500ms AND sin errores de socket"
    result          = $(if ($pass) { "PASS" } else { "FAIL" })
}

$out = "load_test_results_c$Clients.json"
$report | ConvertTo-Json -Depth 4 | Tee-Object -FilePath $out | Out-Null
Write-Host ("RESULT: {0}  clients={1} dur={2}s ok={3} fail={4} p50={5}ms p95={6}ms max={7}ms rps={8}" -f `
    $report.result, $Clients, $DurationSec, $tOk, $tFail, $p50, $p95, $report.latency_ms.max, $report.throughput_rps) `
    -ForegroundColor $(if ($report.result -eq "PASS") { "Green" } else { "Red" })
