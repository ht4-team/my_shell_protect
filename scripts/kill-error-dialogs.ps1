# Kill all error dialogs, WerFault processes, and leftover test processes.
$killed = 0

# 1. Kill WerFault (Windows Error Reporting popup)
Get-Process -Name WerFault -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "Killing WerFault PID=$($_.Id)"
    Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
    $killed++
}

# 2. Enumerate all visible windows and close error dialogs
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
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);

    public static List<Tuple<uint, IntPtr, string>> FindErrorDialogs() {
        var results = new List<Tuple<uint, IntPtr, string>>();
        EnumWindows((hWnd, _) => {
            if (!IsWindowVisible(hWnd)) return true;
            var sb = new System.Text.StringBuilder(512);
            GetWindowTextW(hWnd, sb, 512);
            string title = sb.ToString();
            if (string.IsNullOrEmpty(title)) return true;
            string lower = title.ToLowerInvariant();
            bool isError = lower.Contains("application error")
                || lower.Contains("appcrash")
                || lower.Contains("stopped working")
                || lower.Contains("runtime error")
                || lower.Contains("not responding")
                || lower.Contains("\u5E94\u7528\u7A0B\u5E8F\u9519\u8BEF");
            if (isError) {
                uint pid;
                GetWindowThreadProcessId(hWnd, out pid);
                PostMessage(hWnd, 0x0010, IntPtr.Zero, IntPtr.Zero);
                results.Add(Tuple.Create(pid, hWnd, title));
            }
            return true;
        }, IntPtr.Zero);
        return results;
    }
}
"@

$dialogs = [WinHelper]::FindErrorDialogs()
foreach ($d in $dialogs) {
    Write-Host "Closing dialog: PID=$($d.Item1) title=[$($d.Item3)]"
    try {
        Stop-Process -Id $d.Item1 -Force -ErrorAction SilentlyContinue
    } catch {}
    $killed++
}

# 3. Kill leftover test processes
foreach ($name in @("calc", "myapp", "myapp_con", "test_packed")) {
    Get-Process -Name $name -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "Killing $name PID=$($_.Id)"
        Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
        $killed++
    }
}

Write-Host "Killed $killed processes."
