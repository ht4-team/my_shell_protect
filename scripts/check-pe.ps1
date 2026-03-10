param([string]$Path)
$fs = [System.IO.File]::OpenRead($Path)
$br = New-Object System.IO.BinaryReader($fs)
$fs.Seek(0x3C, [System.IO.SeekOrigin]::Begin) | Out-Null
$elf = $br.ReadInt32()
$fs.Seek($elf + 4, [System.IO.SeekOrigin]::Begin) | Out-Null
$machine = $br.ReadUInt16()
$fs.Close()
Write-Host ("Machine: 0x{0:X4}" -f $machine)
Write-Host ("Size: {0}" -f (Get-Item $Path).Length)
