param([string]$Path, [int]$Seconds = 3)
try {
    $p = Start-Process -FilePath $Path -PassThru
    Start-Sleep -Seconds $Seconds
    if ($p.HasExited) {
        Write-Host "EXITED code=$($p.ExitCode)"
    } else {
        Write-Host "ALIVE (pid=$($p.Id))"
        Stop-Process -Id $p.Id -Force
    }
} catch {
    Write-Host "ERROR: $($_.Exception.Message)"
}
