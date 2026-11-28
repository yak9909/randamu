# DupliX 2: an editor edit shows in the next render without purging AE's caches (tests/ae/dx2_live.jsx).
#   pwsh tests/ae/run_dx2_live.ps1
# Reads the effect's dump (one block per PreRender) and its log: after the edit and after the macro, frame 0 must be
# rendered again with the edited graph (3 x 5 = 15 elements).
param([int]$WaitSec = 5)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$out = Join-Path $root "build\test\dx2_live"
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -File | Remove-Item -Confirm:$false
. (Join-Path $root "spike\tools\ae_bg.ps1")
. (Join-Path $root "spike\tools\ae_guard.ps1")
if (Get-Process AfterFX, aerender -ErrorAction SilentlyContinue) { throw "AfterFX/aerender is already running. Close it first." }
$null = Repair-AeCrashState
$exe = "C:\Program Files\Adobe\Adobe After Effects 2025\Support Files\AfterFX.exe"
$jsx = Join-Path $env:TEMP "duplix_dx2_live.jsx"
Copy-Item (Join-Path $PSScriptRoot "dx2_live.jsx") $jsx -Force
$env:DUPLIX_TEST_DIR = $out -replace '\\', '/'
$env:DUPLIX_LOG = Join-Path $out "plugin.log"
$env:DUPLIX2_DUMP = Join-Path $out "dump.txt"
$env:DUPLIX2_TEST_SCRIPT = Join-Path $PSScriptRoot "dx2_live_script.txt"
$env:DUPLIX2_IDLE_LOG = "1"
$ae = Start-AeHidden -Exe $exe -Arguments ""
Remove-Item env:DUPLIX_LOG, env:DUPLIX2_DUMP, env:DUPLIX2_TEST_SCRIPT, env:DUPLIX2_IDLE_LOG -ErrorAction SilentlyContinue
function Send-Step([string]$step, [int]$sec) {
    Set-Content -Encoding utf8NoBOM (Join-Path $out "step.txt") $step
    Add-Content (Join-Path $out "dump.txt") "== step $step"
    $null = Send-AeScript -Ae $ae -Exe $exe -Jsx $jsx -Done (Join-Path $out "step_done_$step.txt") -TimeoutSec $sec
}
try {
    $ready = $false
    for ($j = 0; $j -lt 80 -and -not $ae.HasExited; $j++) {
        Start-Sleep -Seconds 3
        if (Get-AeHiddenWindows -ProcessId $ae.Id | Where-Object { $_.Visible -and $_.Class -like "AE_CApplication*" }) { $ready = $true; break }
    }
    if (-not $ready) { throw "AE did not finish starting" }
    Start-Sleep -Seconds 3
    Send-Step "setup" 60
    # nobody asks for a frame now: a PreRender here is the comp viewer drawing the edit by itself
    Add-Content (Join-Path $out "dump.txt") "== step viewer"
    Start-Sleep -Seconds $WaitSec  # the idle hook writes the edit meanwhile
    Send-Step "edited" 60
    Start-Sleep -Seconds 2
    Send-Step "macro" 60
} finally {
    if (-not $ae.HasExited) { $null = Stop-AeHidden -Process $ae -TimeoutSec 20 }
    $null = Repair-AeCrashState
}
Get-Content (Join-Path $out "jsx.log")
Select-String -Path (Join-Path $out "plugin.log") -Pattern "dx2: (write|test script|prerender)" | ForEach-Object { $_.Line }
# per step: the element counts of the PreRenders that ran in it
$step = ""; $counts = @{}
foreach ($l in Get-Content (Join-Path $out "dump.txt")) {
    if ($l -like "== step *") { $step = $l.Substring(8); $counts[$step] = @(); continue }
    if ($l -like "t *") { $counts[$step] += 0; continue }
    if ($l -like "e *" -and $counts[$step].Count) { $counts[$step][-1]++ }
}
$ok = $true
foreach ($s in "setup", "viewer", "edited", "macro") {
    $c = $counts[$s]
    Write-Host ("{0}: PreRenders with {1} element(s)" -f $s, ($(if ($c) { $c -join ", " } else { "none" })))
}
if (-not ($counts["viewer"] -contains 15)) { Write-Host "viewer: WRONG (the comp viewer did not draw the edit by itself)"; $ok = $false }
# edited: frame 0 again; when the viewer drew the edit already, AE takes that frame from its cache (no PreRender)
if (-not ($counts["edited"] -contains 15) -and -not (($counts["viewer"] -contains 15) -and $counts["edited"].Count -eq 0)) {
    Write-Host "edited: WRONG (frame 0 not rendered with the edited graph)"; $ok = $false
}
if (-not ($counts["macro"] -contains 15)) { Write-Host "macro: WRONG (the macro's render did not use the edited graph)"; $ok = $false }
Write-Host $(if ($ok) { "DX2 LIVE PASS" } else { "DX2 LIVE FAIL" })
exit $(if ($ok) { 0 } else { 1 })
