# DupliX 2 (design v4 P1): a graph driven by keyed macros, and an editor script written through the idle hook
# (tests/ae/dx2_graph.jsx). AE runs hidden and stays open; the steps are sent as scripts to the running instance,
# so AE's main loop (and its idle hook) runs between them. Then check_dx2_graph.py compares the effect's dump with
# graph_test.exe eval and reads the pixels.
#   pwsh tests/ae/run_dx2_graph.ps1 [-WaitSec 5] [-Bpc 8|16|32]
param([int]$WaitSec = 5, [ValidateSet(8, 16, 32)][int]$Bpc = 8)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$out = Join-Path $root "build\test\dx2_graph"
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -File | Remove-Item -Confirm:$false
. (Join-Path $root "spike\tools\ae_bg.ps1")
. (Join-Path $root "spike\tools\ae_guard.ps1")
if (Get-Process AfterFX, aerender -ErrorAction SilentlyContinue) { throw "AfterFX/aerender is already running. Close it first." }
$null = Repair-AeCrashState
$exe = "C:\Program Files\Adobe\Adobe After Effects 2025\Support Files\AfterFX.exe"
$jsx = Join-Path $env:TEMP "duplix_dx2_graph.jsx"
Copy-Item (Join-Path $PSScriptRoot "dx2_graph.jsx") $jsx -Force

$env:DUPLIX_TEST_DIR = $out -replace '\\', '/'
$env:DUPLIX_LOG = Join-Path $out "plugin.log"
$env:DUPLIX2_DUMP = Join-Path $out "dump.txt"
$env:DUPLIX2_TEST_SCRIPT = Join-Path $PSScriptRoot "dx2_graph_script.txt"
$env:DUPLIX_DX2_BPC = "$Bpc"
$env:DUPLIX2_IDLE_LOG = "1"
$ae = Start-AeHidden -Exe $exe -Arguments ""
Remove-Item env:DUPLIX_LOG, env:DUPLIX2_DUMP, env:DUPLIX2_TEST_SCRIPT, env:DUPLIX_DX2_BPC, env:DUPLIX2_IDLE_LOG -ErrorAction SilentlyContinue

function Send-Step([string]$step, [int]$sec) {
    Set-Content -Encoding utf8NoBOM (Join-Path $out "step.txt") $step
    $tries = Send-AeScript -Ae $ae -Exe $exe -Jsx $jsx -Done (Join-Path $out "step_done_$step.txt") -TimeoutSec $sec
    if ($tries -gt 1) { Write-Host "  step $step ran on try $tries" }
}
# The editor's window (open on comp D) driven with the mouse as a user would (the messages its mouse handlers get):
# drag from the circle's output to the Output's input, then click the circle's Count and type 9 (dx2_graph_script.txt).
function Send-Mouse {
    $w = Get-AeHiddenWindows -ProcessId $ae.Id | Where-Object { $_.Class -eq "DupliX2Editor" -and $_.Visible } | Select-Object -First 1
    if (-not $w) { Write-Host "mouse: no editor window"; return }
    $h = [IntPtr]$w.Hwnd
    function Msg([int]$m, [int]$x, [int]$y, [int]$keys) {
        $null = [AeBg]::PostMessage($h, $m, [IntPtr]$keys, [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF)))
        Start-Sleep -Milliseconds 150
    }
    $DOWN = 0x201; $UP = 0x202; $MOVE = 0x200; $MK_L = 1
    # design v5 P1.5e: select the circle (its title) and open it with Tab (a built-in group: read only), Tab goes back
    Msg $DOWN 120 396 $MK_L
    Msg $UP 120 396 0
    $null = [AeBg]::PostMessage($h, 0x100, [IntPtr]0x09, [IntPtr]0)  # WM_KEYDOWN Tab
    Start-Sleep -Milliseconds 300
    $null = [AeBg]::PostMessage($h, 0x100, [IntPtr]0x09, [IntPtr]0)
    Start-Sleep -Milliseconds 300
    Msg $DOWN 250 418 $MK_L                          # the circle's output
    foreach ($t in 1..5) { Msg $MOVE (250 + 52 * $t) (418 - 54 * $t) $MK_L }
    Msg $UP 524 158 0                                # near the Output's input (510, 148): the wire snaps to it
    Start-Sleep -Seconds 1
    # the edit box a click on a value opens (looked for up to 3 s: AE may be busy with the wire's write)
    function Wait-EditBox {
        foreach ($t in 1..12) {
            Start-Sleep -Milliseconds 250
            $b = [AeText]::FindChild($h, "Edit")
            if ($b -ne [IntPtr]::Zero) { return $b }
        }
        return [IntPtr]::Zero
    }
    Msg $DOWN 200 438 $MK_L                          # the circle's Count
    Msg $UP 200 438 0
    $e = Wait-EditBox
    Write-Host ("mouse: editor {0}, edit box {1}" -f $w.Hwnd, [long]$e)
    if ($e -ne [IntPtr]::Zero) {
        # a formula with names is refused in a value's field (design v5 8.3), then a number goes in
        [AeText]::SetText($e, "m1 * 2")
        $null = [AeBg]::PostMessage($e, 0x100, [IntPtr]0x0D, [IntPtr]0)  # WM_KEYDOWN Enter
        Start-Sleep -Milliseconds 500
        Msg $DOWN 200 438 $MK_L
        Msg $UP 200 438 0
        $e = Wait-EditBox
    }
    if ($e -ne [IntPtr]::Zero) {
        [AeText]::SetText($e, "9")
        $null = [AeBg]::PostMessage($e, 0x100, [IntPtr]0x0D, [IntPtr]0)  # WM_KEYDOWN Enter
    }
    Start-Sleep -Seconds 3  # the idle hook writes meanwhile
    # the Code view (design v5 8.4): code that does not read is refused (the graph stays), then a circle of 6 applied
    Msg $DOWN 200 13 $MK_L                           # the Code tab
    Msg $UP 200 13 0
    Start-Sleep -Milliseconds 500
    $code = [AeText]::FindChildByStyle($h, "Edit", 0x4)   # ES_MULTILINE
    $apply = [AeText]::FindChild($h, "Button")
    Write-Host ("code: edit {0}, apply {1}" -f [long]$code, [long]$apply)
    if ($code -ne [IntPtr]::Zero -and $apply -ne [IntPtr]::Zero) {
        [AeText]::SetText($code, "x = nope`r`n")
        $null = [AeBg]::PostMessage($apply, 0x00F5, [IntPtr]0, [IntPtr]0)  # BM_CLICK
        Start-Sleep -Seconds 1
        [AeText]::SetText($code, "c = circle(count: 6, radius: 80)`r`noutput(c, copy_scale: 50)`r`n")
        $null = [AeBg]::PostMessage($apply, 0x00F5, [IntPtr]0, [IntPtr]0)
        Start-Sleep -Seconds 3  # the idle hook writes meanwhile
    }
    # back to the Nodes view; the wheel zooms about the cursor (3 notches: x 1.1^3)
    Msg $DOWN 44 13 $MK_L                            # the Nodes tab
    Msg $UP 44 13 0
    # WM_MOUSEWHEEL takes screen coordinates: the client point (300, 300) on the window
    $pt = New-Object AeBg+PT
    $null = [AeBg]::ClientToScreen($h, [ref]$pt)
    $sx = $pt.X + 300; $sy = $pt.Y + 300
    foreach ($t in 1..3) {
        $null = [AeBg]::PostMessage($h, 0x020A, [IntPtr](120 -shl 16), [IntPtr]((($sy -band 0xFFFF) -shl 16) -bor ($sx -band 0xFFFF)))
        Start-Sleep -Milliseconds 150
    }
    Start-Sleep -Milliseconds 500
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
    Start-Sleep -Seconds $WaitSec  # the idle hook writes the editor script meanwhile
    Send-Mouse
    Send-Step "check" 120
    Start-Sleep -Seconds 3  # the editor notices the undo meanwhile
    Send-Step "close" 60
    Start-Sleep -Seconds 3  # the editor closes meanwhile
} finally {
    foreach ($d in Get-AeHiddenDialogs) { Write-Host ("dialog: {0} [{1}] {2}" -f $d.Process, $d.Title, $d.Text) }
    if (-not $ae.HasExited) { $null = Stop-AeHidden -Process $ae -TimeoutSec 20 }
    $null = Repair-AeCrashState
}
Get-Content (Join-Path $out "jsx.log") | Where-Object { $_ -notmatch '^ #' }
Select-String -Path (Join-Path $out "plugin.log") -Pattern "dx2: (write|test script|preset|GlobalSetup|graph:|arb:|editor)" | ForEach-Object { $_.Line }
& (Join-Path $root "spike\.venv\Scripts\python.exe") -I (Join-Path $PSScriptRoot "check_dx2_graph.py") $out
exit $LASTEXITCODE
