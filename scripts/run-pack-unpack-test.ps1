param(
    [string]$Platform = "Win32",
    [string]$Configuration = "Release",
    [string]$CalcPath = "",
    [switch]$CalcMustStayRunning
)

$ErrorActionPreference = "Stop"

function Resolve-MSBuildPath {
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($vsPath) {
            $candidate = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    throw "MSBuild.exe not found. Install Visual Studio Build Tools or add MSBuild to PATH."
}

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

function Invoke-NativeChecked {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments
    )
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed: $FilePath $($Arguments -join ' ') (exit=$LASTEXITCODE)"
    }
}

function Test-ProcessLiveness {
    param([Parameter(Mandatory = $true)][string]$Path)

    $proc = Start-Process -FilePath $Path -PassThru
    Start-Sleep -Seconds 2
    $alive = -not $proc.HasExited
    $exitCode = $null
    if ($proc.HasExited) {
        $exitCode = $proc.ExitCode
    } else {
        Stop-Process -Id $proc.Id -Force
    }
    return [PSCustomObject]@{
        Alive = $alive
        ExitCode = $exitCode
    }
}

function Assert-CalcLaunch {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][bool]$RequireAlive
    )
    $state = Test-ProcessLiveness $Path
    if ($RequireAlive) {
        if (-not $state.Alive) {
            throw "calc process is not alive after launch: $Path (exit=$($state.ExitCode))"
        }
        return
    }
    if (($null -ne $state.ExitCode) -and ($state.ExitCode -ne 0)) {
        throw "calc exited with non-zero code: $Path (exit=$($state.ExitCode))"
    }
}

function Stop-ProcessByPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (!(Test-Path $Path)) {
        return
    }
    $target = (Resolve-Path $Path).Path
    $name = [System.IO.Path]::GetFileNameWithoutExtension($target)
    $procs = Get-Process -Name $name -ErrorAction SilentlyContinue
    foreach ($p in $procs) {
        try {
            if ($p.Path -eq $target) {
                Stop-Process -Id $p.Id -Force
            }
        } catch {
        }
    }
}

function Get-PeMachine {
    param([Parameter(Mandatory = $true)][string]$Path)
    $fs = [System.IO.File]::OpenRead($Path)
    try {
        $br = New-Object System.IO.BinaryReader($fs)
        $fs.Seek(0x3C, [System.IO.SeekOrigin]::Begin) | Out-Null
        $e_lfanew = $br.ReadInt32()
        $fs.Seek($e_lfanew + 4, [System.IO.SeekOrigin]::Begin) | Out-Null
        return $br.ReadUInt16()
    } finally {
        $fs.Close()
    }
}

Write-Host "[1/4] Build binaries"
$msbuild = Resolve-MSBuildPath
Write-Host "Using MSBuild: $msbuild"
Invoke-NativeChecked $msbuild .\CombatShell\CombatShell.vcxproj /m /p:Configuration=$Configuration /p:Platform=$Platform
Invoke-NativeChecked $msbuild .\CombatShellCli.vcxproj /m /p:Configuration=$Configuration /p:Platform=$Platform
Invoke-NativeChecked $msbuild .\examples\MiniTarget.vcxproj /m /p:Configuration=$Configuration /p:Platform=$Platform

$binDir = if ($Platform -eq "x64") { "bin\\x64" } else { "bin" }
if (!(Test-Path $binDir)) {
    throw "Build output directory not found: $binDir"
}

Write-Host "[2/4] Prepare test workspace"
$sampleDir = "test\\samples_$Platform"
New-Item -ItemType Directory -Force -Path $sampleDir | Out-Null

if ($CalcPath) {
    if (!(Test-Path $CalcPath)) {
        throw "CalcPath not found: $CalcPath"
    }
    Copy-Item $CalcPath "$sampleDir\\calc.exe" -Force
} elseif (Test-Path "examples\\calc.exe") {
    Copy-Item "examples\\calc.exe" "$sampleDir\\calc.exe" -Force
} elseif (Test-Path "$binDir\\MiniTarget.exe") {
    Copy-Item "$binDir\\MiniTarget.exe" "$sampleDir\\calc.exe" -Force
} else {
    throw "No calc sample available. Pass -CalcPath explicitly."
}
if (Test-Path "$binDir\\MiniTarget.exe") {
    Copy-Item "$binDir\\MiniTarget.exe" "$sampleDir\\mini_target.exe" -Force
}
Copy-Item "$binDir\\CombatShellCli.exe" "$sampleDir\\CombatShellCli.exe" -Force
Copy-Item "$binDir\\CombatShell.dll" "$sampleDir\\CombatShell.dll" -Force

$calcPath = (Resolve-Path "$sampleDir\\calc.exe").Path
$expectedMachine = if ($Platform -eq "x64") { 0x8664 } else { 0x014C }
$calcMachine = Get-PeMachine $calcPath
$runCalcTest = ($calcMachine -eq $expectedMachine)
if (-not $runCalcTest) {
    Write-Host "Skip calc test: calc machine=0x$('{0:X4}' -f $calcMachine), platform=$Platform"
}

Push-Location $sampleDir
try {
    Write-Host "[3/4] Baseline checks"
    $calcRequireAlive = $false
    if ($runCalcTest) {
        $calcState = Test-ProcessLiveness ".\\calc.exe"
        if ($CalcMustStayRunning.IsPresent) {
            if (-not $calcState.Alive) {
                throw "calc baseline is not alive; cannot satisfy -CalcMustStayRunning for .\\calc.exe"
            }
            $calcRequireAlive = $true
        } else {
            $calcRequireAlive = $calcState.Alive
        }
        if ($calcRequireAlive) {
            Write-Host "calc check mode: require process alive"
        } else {
            Write-Host "calc check mode: allow quick-exit (baseline behavior)"
            if (($null -ne $calcState.ExitCode) -and ($calcState.ExitCode -ne 0)) {
                throw "calc baseline failed: .\\calc.exe (exit=$($calcState.ExitCode))"
            }
        }
    }

    $runMiniTarget = $false
    if (Test-Path ".\\mini_target.exe") {
        $miniMachine = Get-PeMachine (Resolve-Path ".\\mini_target.exe").Path
        $runMiniTarget = ($miniMachine -eq $expectedMachine)
        if (-not $runMiniTarget) {
            Write-Host "Skip mini_target test: machine=0x$('{0:X4}' -f $miniMachine), platform=$Platform"
        }
    }
    if ($runMiniTarget) {
        Assert-ProgramOutput ".\\mini_target.exe" "mini-target-ok"
    }

    Write-Host "[4/4] Pack/Unpack checks"
    if ($runCalcTest) {
        .\CombatShellCli.exe pack .\calc.exe
        if ($LASTEXITCODE -ne 0) { throw "pack calc failed with $LASTEXITCODE" }
        Assert-CalcLaunch ".\\calc.exe" $calcRequireAlive
        .\CombatShellCli.exe unpack .\calc.exe
        if ($LASTEXITCODE -ne 0) { throw "unpack calc failed with $LASTEXITCODE" }
        Assert-CalcLaunch ".\\calc.exe" $calcRequireAlive
    }

    if ($runMiniTarget) {
        .\CombatShellCli.exe pack .\mini_target.exe
        if ($LASTEXITCODE -ne 0) { throw "pack mini_target failed with $LASTEXITCODE" }
        Assert-ProgramOutput ".\\mini_target.exe" "mini-target-ok"
        .\CombatShellCli.exe unpack .\mini_target.exe
        if ($LASTEXITCODE -ne 0) { throw "unpack mini_target failed with $LASTEXITCODE" }
        Assert-ProgramOutput ".\\mini_target.exe" "mini-target-ok"
    }
}
finally {
    Stop-ProcessByPath ".\\calc.exe"
    Pop-Location
}

Write-Host "All tests passed."
