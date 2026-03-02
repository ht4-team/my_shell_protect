param(
    [string]$Platform = "Win32",
    [string]$Configuration = "Release"
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
& $msbuild .\CombatShell\CombatShell.vcxproj /m /p:Configuration=$Configuration /p:Platform=$Platform
& $msbuild .\CombatShellCli.vcxproj /m /p:Configuration=$Configuration /p:Platform=$Platform
if ($Platform -eq "Win32") {
    & $msbuild .\examples\MiniTarget.vcxproj /m /p:Configuration=$Configuration /p:Platform=$Platform
}

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
    if ($runCalcTest) {
        Assert-CalcLaunch ".\\calc.exe"
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
        Assert-CalcLaunch ".\\calc.exe"
        .\CombatShellCli.exe unpack .\calc.exe
        if ($LASTEXITCODE -ne 0) { throw "unpack calc failed with $LASTEXITCODE" }
        Assert-CalcLaunch ".\\calc.exe"
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
    Pop-Location
}

Write-Host "All tests passed."
