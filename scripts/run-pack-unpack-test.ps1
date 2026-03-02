param(
    [string]$Platform = "Win32",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

function Assert-ProgramOutput {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ExpectedPattern
    )
    $output = & $Path
    if ($LASTEXITCODE -ne 0) {
        throw "Program failed: $Path (exit=$LASTEXITCODE)"
    }
    if ($output -notmatch $ExpectedPattern) {
        throw "Output mismatch: $Path, output='$output'"
    }
}

function Assert-CalcLaunch {
    param([Parameter(Mandatory = $true)][string]$Path)
    $proc = Start-Process -FilePath $Path -PassThru
    Start-Sleep -Seconds 2
    if ($proc.HasExited) {
        if ($proc.ExitCode -ne 0) {
            throw "calc exited with non-zero code: $Path (exit=$($proc.ExitCode))"
        }
        return
    }
    Stop-Process -Id $proc.Id -Force
}

Write-Host "[1/4] Build binaries"
msbuild .\CombatShell\CombatShell.vcxproj /m /p:Configuration=$Configuration /p:Platform=$Platform
msbuild .\CombatShellCli.vcxproj /m /p:Configuration=$Configuration /p:Platform=$Platform
msbuild .\examples\MiniTarget.vcxproj /m /p:Configuration=$Configuration /p:Platform=$Platform

$binDir = if ($Platform -eq "x64") { "bin\\x64" } else { "bin" }
if (!(Test-Path $binDir)) {
    throw "Build output directory not found: $binDir"
}

Write-Host "[2/4] Prepare test workspace"
$sampleDir = "test\\samples"
New-Item -ItemType Directory -Force -Path $sampleDir | Out-Null

if (Test-Path "examples\\calc.exe") {
    Copy-Item "examples\\calc.exe" "$sampleDir\\calc.exe" -Force
} else {
    Copy-Item "$binDir\\MiniTarget.exe" "$sampleDir\\calc.exe" -Force
}
Copy-Item "$binDir\\MiniTarget.exe" "$sampleDir\\mini_target.exe" -Force
Copy-Item "$binDir\\CombatShellCli.exe" "$sampleDir\\CombatShellCli.exe" -Force
Copy-Item "$binDir\\CombatShell.dll" "$sampleDir\\CombatShell.dll" -Force

Push-Location $sampleDir
try {
    Write-Host "[3/4] Baseline checks"
    Assert-CalcLaunch ".\\calc.exe"
    Assert-ProgramOutput ".\\mini_target.exe" "mini-target-ok"

    Write-Host "[4/4] Pack/Unpack checks"
    .\CombatShellCli.exe pack .\calc.exe
    if ($LASTEXITCODE -ne 0) { throw "pack calc failed with $LASTEXITCODE" }
    Assert-CalcLaunch ".\\calc.exe"
    .\CombatShellCli.exe unpack .\calc.exe
    if ($LASTEXITCODE -ne 0) { throw "unpack calc failed with $LASTEXITCODE" }
    Assert-CalcLaunch ".\\calc.exe"

    .\CombatShellCli.exe pack .\mini_target.exe
    if ($LASTEXITCODE -ne 0) { throw "pack mini_target failed with $LASTEXITCODE" }
    Assert-ProgramOutput ".\\mini_target.exe" "mini-target-ok"
    .\CombatShellCli.exe unpack .\mini_target.exe
    if ($LASTEXITCODE -ne 0) { throw "unpack mini_target failed with $LASTEXITCODE" }
    Assert-ProgramOutput ".\\mini_target.exe" "mini-target-ok"
}
finally {
    Pop-Location
}

Write-Host "All tests passed."
