# Kill all Application Error / WerFault dialogs and any leftover packed-calc processes.
$killed = 0

# 1. Kill WerFault (Windows Error Reporting popup)
Get-Process -Name WerFault -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "Killing WerFault PID=$($_.Id)"
    Stop-Process -Id $_.Id -Force
    $killed++
}

# 2. Kill any window with "Application Error" or "应用程序错误" in the title
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
public class WinHelper {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lParam);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr hWnd, System.Text.StringBuilder sb, int max);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    public static List<uint> FindErrorDialogs() {
        var pids = new List<uint>();
        EnumWindows((hWnd, _) => {
            var sb = new System.Text.StringBuilder(512);
            GetWindowTextW(hWnd, sb, 512);
            string title = sb.ToString();
            if (title.Contains("Application Error") || title.Contains("应用程序错误") || title.Contains("error") && title.Length < 60) {
                uint pid;
                GetWindowThreadProcessId(hWnd, out pid);
                pids.Add(pid);
                // Send WM_CLOSE
                PostMessage(hWnd, 0x0010, IntPtr.Zero, IntPtr.Zero);
            }
            return true;
        }, IntPtr.Zero);
        return pids;
    }
}
"@

$errorPids = [WinHelper]::FindErrorDialogs()
foreach ($epid in ($errorPids | Sort-Object -Unique)) {
    Write-Host "Closing error dialog PID=$epid"
    try { Stop-Process -Id $epid -Force -ErrorAction SilentlyContinue } catch {}
    $killed++
}

# 3. Kill leftover calc.exe
Get-Process -Name calc -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "Killing calc PID=$($_.Id)"
    Stop-Process -Id $_.Id -Force
    $killed++
}

Write-Host "Killed $killed processes."
