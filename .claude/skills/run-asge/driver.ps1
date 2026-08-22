<#
.SYNOPSIS
  Drives a built ASGE example (or any Win32/SDL window app) headfully:
  launch, screenshot, send keys, check status, stop.

.DESCRIPTION
  ASGE examples are native Win32 windows (SDL3 + a GDI/software
  backend), not a browser or Electron app, so there is no CDP/DevTools
  handle to attach to. This script is the handle: it launches the exe,
  waits for its window, and drives it via the Win32 API (GetWindowRect,
  SetForegroundWindow) plus System.Windows.Forms.SendKeys for input and
  System.Drawing for screenshots.

.PARAMETER Action
  launch | screenshot | sendkeys | status | stop

.PARAMETER Exe
  Path to the .exe to launch (launch only).

.PARAMETER ProcessId
  PID of a previously-launched process (screenshot/sendkeys/status/stop).
  Defaults to the PID recorded by the last `launch` call (see -PidFile).

.PARAMETER Out
  Output PNG path (screenshot only).

.PARAMETER Keys
  SendKeys-format key string, e.g. "d" or "{ESC}" or "w{ENTER}" (sendkeys only).

.PARAMETER PidFile
  Where `launch` records the PID for later calls to default to.
  Defaults to $env:TEMP\asge_driver_pid.txt.

.EXAMPLE
  powershell -File driver.ps1 launch -Exe C:\path\to\bin\Debug\ecs_demo.exe
  powershell -File driver.ps1 sendkeys -Keys "d"
  powershell -File driver.ps1 screenshot -Out C:\shots\ecs_demo.png
  powershell -File driver.ps1 stop
#>
param(
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet("launch","screenshot","sendkeys","status","stop")]
    [string]$Action,

    [string]$Exe,
    [int]$ProcessId = 0,
    [string]$Out,
    [string]$Keys,
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
}
"@

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
        if ($p.MainWindowHandle -eq 0) { throw "Process has no main window." }
        [ASGE_Win32]::ShowWindow($p.MainWindowHandle, 9) | Out-Null   # SW_RESTORE
        [ASGE_Win32]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
        Start-Sleep -Milliseconds 200
        [System.Windows.Forms.SendKeys]::SendWait($Keys)
        Write-Output "sent '$Keys'"
    }

    "screenshot" {
        if (-not $Out) { throw "screenshot requires -Out <path.png>" }
        $p = Get-Process -Id (Resolve-Pid)
        if ($p.MainWindowHandle -eq 0) { throw "Process has no main window." }

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
