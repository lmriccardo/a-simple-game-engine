<#
.SYNOPSIS
  Drives a built ASGE example (or any Win32/SDL window app) headfully:
  launch, screenshot, send keys, move/click/drag the mouse, check status, stop.

.DESCRIPTION
  ASGE examples are native Win32 windows (SDL3 + a GDI/software
  backend), not a browser or Electron app, so there is no CDP/DevTools
  handle to attach to. This script is the handle: it launches the exe,
  waits for its window, and drives it via the Win32 API (GetWindowRect,
  SetForegroundWindow, SetCursorPos/mouse_event) plus
  System.Windows.Forms.SendKeys for keyboard input and System.Drawing for
  screenshots.

.PARAMETER Action
  launch | screenshot | sendkeys | mousemove | click | drag | status | stop

.PARAMETER Exe
  Path to the .exe to launch (launch only).

.PARAMETER ProcessId
  PID of a previously-launched process (screenshot/sendkeys/mousemove/click/drag/status/stop).
  Defaults to the PID recorded by the last `launch` call (see -PidFile).

.PARAMETER Out
  Output PNG path (screenshot only).

.PARAMETER Keys
  SendKeys-format key string, e.g. "d" or "{ESC}" or "w{ENTER}" (sendkeys only).

.PARAMETER X
  Target X coordinate for mousemove/click/drag, relative to the window's own
  top-left corner (the SAME origin `screenshot` captures from) -- read a pixel
  position straight off a screenshot and pass it here with no conversion.
  For `drag`, this is the press/start point.

.PARAMETER Y
  Target Y coordinate -- see -X.

.PARAMETER X2
  drag only: the release/end point's X coordinate (same window-relative origin as -X).

.PARAMETER Y2
  drag only: the release/end point's Y coordinate -- see -X2.

.PARAMETER Button
  click/drag only: left | right | middle. Defaults to left.

.PARAMETER Steps
  drag only: how many intermediate mouse-move events to send between the
  start and end point (default 12) -- several small steps, not one jump,
  since some widgets (sliders, gizmo drags) respond to per-event motion
  deltas rather than just the final position.

.PARAMETER PidFile
  Where `launch` records the PID for later calls to default to.
  Defaults to $env:TEMP\asge_driver_pid.txt.

.EXAMPLE
  powershell -File driver.ps1 launch -Exe C:\path\to\bin\Debug\ecs_demo.exe
  powershell -File driver.ps1 sendkeys -Keys "d"
  powershell -File driver.ps1 click -X 120 -Y 340
  powershell -File driver.ps1 drag -X 200 -Y 200 -X2 260 -Y2 260
  powershell -File driver.ps1 screenshot -Out C:\shots\ecs_demo.png
  powershell -File driver.ps1 stop
#>
param(
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet("launch","screenshot","sendkeys","mousemove","click","drag","status","stop")]
    [string]$Action,

    [string]$Exe,
    [int]$ProcessId = 0,
    [string]$Out,
    [string]$Keys,
    [int]$X,
    [int]$Y,
    [int]$X2,
    [int]$Y2,
    [ValidateSet("left","right","middle")]
    [string]$Button = "left",
    [int]$Steps = 12,
    [string]$PidFile = "$env:TEMP\asge_driver_pid.txt"
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -ErrorAction SilentlyContinue @"
using System;
using System.Runtime.InteropServices;
public struct ASGE_RECT { public int Left; public int Top; public int Right; public int Bottom; }
public class ASGE_Win32 {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out ASGE_RECT lpRect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, int dx, int dy, uint dwData, UIntPtr dwExtraInfo);
}
"@

# mouse_event button-state flags -- see MOUSEEVENTF_* in winuser.h. Only
# DOWN/UP are used (never the MOVE flag): SetCursorPos positions the cursor
# first, then these fire a button transition at wherever it already is.
$script:MouseDownFlag = @{ left = 0x0002; right = 0x0008; middle = 0x0020 }
$script:MouseUpFlag   = @{ left = 0x0004; right = 0x0010; middle = 0x0040 }

function Get-WindowRect($p) {
    if ($p.MainWindowHandle -eq 0) { throw "Process has no main window." }
    $rect = New-Object ASGE_RECT
    [ASGE_Win32]::GetWindowRect($p.MainWindowHandle, [ref]$rect) | Out-Null
    return $rect
}

function Focus-Window($p) {
    [ASGE_Win32]::ShowWindow($p.MainWindowHandle, 9) | Out-Null   # SW_RESTORE
    [ASGE_Win32]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 200
}

# Moves the cursor to (inX, inY) relative to inRect's top-left -- the same
# window-relative origin `screenshot` captures from.
function Move-CursorTo($rect, [int]$inX, [int]$inY) {
    [ASGE_Win32]::SetCursorPos($rect.Left + $inX, $rect.Top + $inY) | Out-Null
}

function Resolve-Pid {
    if ($ProcessId -ne 0) { return $ProcessId }
    if (Test-Path $PidFile) { return [int](Get-Content $PidFile) }
    throw "No -ProcessId given and no PID file at $PidFile - run 'launch' first."
}

function Wait-MainWindow($proc, [int]$timeoutSeconds = 10) {
    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $proc.Refresh()
        if ($proc.HasExited) { throw "Process exited before a window appeared (exit code $($proc.ExitCode))." }
        if ($proc.MainWindowHandle -ne 0) { return }
        Start-Sleep -Milliseconds 150
    }
    throw "Timed out waiting for a main window (PID $($proc.Id))."
}

switch ($Action) {
    "launch" {
        if (-not $Exe) { throw "launch requires -Exe <path-to-exe>" }
        if (-not (Test-Path $Exe)) { throw "Exe not found: $Exe" }

        $proc = Start-Process -FilePath $Exe -PassThru -WorkingDirectory (Split-Path $Exe)
        Wait-MainWindow $proc
        $proc.Id | Out-File -FilePath $PidFile -Encoding ascii -NoNewline
        Write-Output "launched pid=$($proc.Id) title='$($proc.MainWindowTitle)'"
    }

    "status" {
        $p = Get-Process -Id (Resolve-Pid) -ErrorAction SilentlyContinue
        if (-not $p) { Write-Output "not running"; break }
        $p.Refresh()
        Write-Output "running pid=$($p.Id) title='$($p.MainWindowTitle)' handle=$($p.MainWindowHandle)"
    }

    "sendkeys" {
        if (-not $Keys) { throw "sendkeys requires -Keys '<sendkeys-string>'" }
        $p = Get-Process -Id (Resolve-Pid)
        Focus-Window $p
        [System.Windows.Forms.SendKeys]::SendWait($Keys)
        Write-Output "sent '$Keys'"
    }

    "mousemove" {
        $p = Get-Process -Id (Resolve-Pid)
        $rect = Get-WindowRect $p
        Focus-Window $p
        Move-CursorTo $rect $X $Y
        Write-Output "moved to ($X, $Y)"
    }

    "click" {
        $p = Get-Process -Id (Resolve-Pid)
        $rect = Get-WindowRect $p
        Focus-Window $p
        Move-CursorTo $rect $X $Y
        Start-Sleep -Milliseconds 60
        [ASGE_Win32]::mouse_event($script:MouseDownFlag[$Button], 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 60
        [ASGE_Win32]::mouse_event($script:MouseUpFlag[$Button], 0, 0, 0, [UIntPtr]::Zero)
        Write-Output "$Button-clicked ($X, $Y)"
    }

    "drag" {
        $p = Get-Process -Id (Resolve-Pid)
        $rect = Get-WindowRect $p
        Focus-Window $p
        Move-CursorTo $rect $X $Y
        Start-Sleep -Milliseconds 60
        [ASGE_Win32]::mouse_event($script:MouseDownFlag[$Button], 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 60

        # Several small steps, not one jump -- some widgets (sliders, gizmo
        # drags) respond to per-event motion deltas, not just the final position.
        for ($i = 1; $i -le $Steps; $i++) {
            $t = $i / [double]$Steps
            $stepX = [int]([math]::Round($X + ($X2 - $X) * $t))
            $stepY = [int]([math]::Round($Y + ($Y2 - $Y) * $t))
            Move-CursorTo $rect $stepX $stepY
            Start-Sleep -Milliseconds 20
        }

        Start-Sleep -Milliseconds 60
        [ASGE_Win32]::mouse_event($script:MouseUpFlag[$Button], 0, 0, 0, [UIntPtr]::Zero)
        Write-Output "$Button-dragged ($X, $Y) -> ($X2, $Y2)"
    }

    "screenshot" {
        if (-not $Out) { throw "screenshot requires -Out <path.png>" }
        $p = Get-Process -Id (Resolve-Pid)
        if ($p.MainWindowHandle -eq 0) { throw "Process has no main window." }
        # CopyFromScreen grabs whatever's actually composited on the physical
        # screen at this rect, not the window specifically -- without raising
        # it first, an occluding window (another app, a video call) gets
        # captured instead if it happens to be on top.
        Focus-Window $p

        $rect = New-Object ASGE_RECT
        [ASGE_Win32]::GetWindowRect($p.MainWindowHandle, [ref]$rect) | Out-Null
        $width  = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        if ($width -le 0 -or $height -le 0) { throw "Window has zero/negative size - is it minimized?" }

        $bmp = New-Object System.Drawing.Bitmap $width, $height
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bmp.Size)
        New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null
        $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
        $g.Dispose(); $bmp.Dispose()
        Write-Output "saved $Out (${width}x${height})"
    }

    "stop" {
        $target = Resolve-Pid
        Stop-Process -Id $target -Force -ErrorAction SilentlyContinue
        Remove-Item $PidFile -ErrorAction SilentlyContinue
        Write-Output "stopped pid=$target"
    }
}
