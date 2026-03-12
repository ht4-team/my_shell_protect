$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$depRoot = Join-Path $repoRoot "dep_build"
$srcRoot = Join-Path $depRoot "src"
$buildRoot = Join-Path $depRoot "build"

$capstoneTag = "4.0.2"
$lz4Tag = "v1.9.2"

$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
$msbuild = "C:\Windows\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe"

function Ensure-Dir([string]$path) {
  if (-not (Test-Path $path)) {
    New-Item -ItemType Directory -Path $path | Out-Null
  }
}

function Invoke-Cmd([string]$cmdLine) {
  cmd /c $cmdLine
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed: $cmdLine"
  }
}

Ensure-Dir $srcRoot
Ensure-Dir $buildRoot

if (-not (Test-Path (Join-Path $srcRoot "capstone"))) {
  git clone --branch $capstoneTag --depth 1 https://github.com/capstone-engine/capstone.git (Join-Path $srcRoot "capstone")
}
if (-not (Test-Path (Join-Path $srcRoot "lz4"))) {
  git clone --branch $lz4Tag --depth 1 https://github.com/lz4/lz4.git (Join-Path $srcRoot "lz4")
}

$capX86Build = Join-Path $buildRoot "capstone_x86"
$capX64Build = Join-Path $buildRoot "capstone_x64"
Ensure-Dir $capX86Build
Ensure-Dir $capX64Build

Invoke-Cmd ('"{0}" x86 && cd /d "{1}" && cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCAPSTONE_BUILD_STATIC_RUNTIME=ON -DCAPSTONE_BUILD_TESTS=OFF -DCAPSTONE_BUILD_CSTOOL=OFF "..\..\src\capstone" && nmake' -f $vcvars, $capX86Build)
Invoke-Cmd ('"{0}" x86_amd64 && cd /d "{1}" && cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCAPSTONE_BUILD_STATIC_RUNTIME=ON -DCAPSTONE_BUILD_TESTS=OFF -DCAPSTONE_BUILD_CSTOOL=OFF "..\..\src\capstone" && nmake' -f $vcvars, $capX64Build)

$lz4Sln = Join-Path $srcRoot "lz4\visual\VS2010\lz4.sln"
Invoke-Cmd ('"{0}" x86 && "{1}" "{2}" /t:liblz4;liblz4-dll /p:Configuration=Release;Platform=Win32 /m' -f $vcvars, $msbuild, $lz4Sln)
Invoke-Cmd ('"{0}" x86_amd64 && "{1}" "{2}" /t:liblz4;liblz4-dll /p:Configuration=Release;Platform=x64 /m' -f $vcvars, $msbuild, $lz4Sln)

$capOut32 = Join-Path $depRoot "capstone\32"
$capOut64 = Join-Path $depRoot "capstone\64"
$capOutRootInclude = Join-Path $depRoot "capstone\include"
$capOut64Include = Join-Path $depRoot "capstone\64\include"

Ensure-Dir $capOut32
Ensure-Dir $capOut64
Ensure-Dir $capOutRootInclude
Ensure-Dir $capOut64Include

Copy-Item (Join-Path $capX86Build "capstone.dll") (Join-Path $capOut32 "capstone.dll") -Force
Copy-Item (Join-Path $capX86Build "capstone.lib") (Join-Path $capOut32 "capstone.lib") -Force
Copy-Item (Join-Path $capX86Build "capstone_dll.lib") (Join-Path $capOut32 "capstone_dll.lib") -Force

Copy-Item (Join-Path $capX64Build "capstone.dll") (Join-Path $capOut64 "capstone.dll") -Force
Copy-Item (Join-Path $capX64Build "capstone.lib") (Join-Path $capOut64 "capstone.lib") -Force
Copy-Item (Join-Path $capX64Build "capstone_dll.lib") (Join-Path $capOut64 "capstone_dll.lib") -Force

$capDocs = @("ChangeLog", "CREDITS.TXT", "LICENSE.TXT", "LICENSE_LLVM.TXT", "README.md", "RELEASE_NOTES", "SPONSORS.TXT")
foreach ($doc in $capDocs) {
  Copy-Item (Join-Path $srcRoot "capstone\$doc") (Join-Path $capOut32 $doc) -Force
  Copy-Item (Join-Path $srcRoot "capstone\$doc") (Join-Path $capOut64 $doc) -Force
}

Copy-Item (Join-Path $srcRoot "capstone\include\*") $capOutRootInclude -Recurse -Force
Copy-Item (Join-Path $srcRoot "capstone\include\*") $capOut64Include -Recurse -Force

$lz4OutStatic = Join-Path $depRoot "lz4\static"
$lz4OutDll = Join-Path $depRoot "lz4\dll"
$lz4OutInclude = Join-Path $depRoot "lz4\include"
$lz4OutExample = Join-Path $depRoot "lz4\example"

Ensure-Dir $lz4OutStatic
Ensure-Dir $lz4OutDll
Ensure-Dir $lz4OutInclude
Ensure-Dir $lz4OutExample

$lz4X64Bin = Join-Path $srcRoot "lz4\visual\VS2010\bin\x64_Release"
Copy-Item (Join-Path $lz4X64Bin "liblz4_static.lib") (Join-Path $lz4OutStatic "liblz4_static.lib") -Force
Copy-Item (Join-Path $lz4X64Bin "liblz4.dll") (Join-Path $lz4OutDll "liblz4.dll") -Force
Copy-Item (Join-Path $lz4X64Bin "liblz4.lib") (Join-Path $lz4OutDll "liblz4.lib") -Force

# Keep filename compatibility with the existing tree.
Copy-Item (Join-Path $lz4X64Bin "liblz4.lib") (Join-Path $lz4OutDll "liblz4.dll.a") -Force

Copy-Item (Join-Path $srcRoot "lz4\NEWS") (Join-Path $depRoot "lz4\NEWS") -Force
Copy-Item (Join-Path $srcRoot "lz4\README.md") (Join-Path $depRoot "lz4\README.md") -Force

Copy-Item (Join-Path $srcRoot "lz4\lib\lz4.c") (Join-Path $lz4OutInclude "lz4.c") -Force
Copy-Item (Join-Path $srcRoot "lz4\lib\lz4.h") (Join-Path $lz4OutInclude "lz4.h") -Force
Copy-Item (Join-Path $srcRoot "lz4\lib\lz4frame.h") (Join-Path $lz4OutInclude "lz4frame.h") -Force
Copy-Item (Join-Path $srcRoot "lz4\lib\lz4hc.h") (Join-Path $lz4OutInclude "lz4hc.h") -Force

Copy-Item (Join-Path $srcRoot "lz4\lib\xxhash.c") (Join-Path $lz4OutExample "xxhash.c") -Force
Copy-Item (Join-Path $srcRoot "lz4\lib\xxhash.h") (Join-Path $lz4OutExample "xxhash.h") -Force
Copy-Item (Join-Path $srcRoot "lz4\tests\fullbench.c") (Join-Path $lz4OutExample "fullbench.c") -Force

# Reuse local templates to keep the exact prebuilt-like example file names.
Copy-Item (Join-Path $repoRoot "lz4\example\Makefile") (Join-Path $lz4OutExample "Makefile") -Force
Copy-Item (Join-Path $repoRoot "lz4\example\fullbench-dll.sln") (Join-Path $lz4OutExample "fullbench-dll.sln") -Force
Copy-Item (Join-Path $repoRoot "lz4\example\fullbench-dll.vcxproj") (Join-Path $lz4OutExample "fullbench-dll.vcxproj") -Force

Write-Host "Done. Output ready under $depRoot\capstone and $depRoot\lz4"
