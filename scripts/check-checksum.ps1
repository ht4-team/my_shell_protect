param([string]$Path)
$fs = [System.IO.File]::OpenRead($Path)
$br = New-Object System.IO.BinaryReader($fs)
# Read e_lfanew
$fs.Seek(0x3C, [System.IO.SeekOrigin]::Begin) | Out-Null
$elf = $br.ReadInt32()
# CheckSum is at OptionalHeader offset 64 (0x40) for PE32+
# NT signature(4) + FileHeader(20) + OptionalHeader starts
# CheckSum offset in OptionalHeader = 64
$checksumOffset = $elf + 4 + 20 + 64
$fs.Seek($checksumOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
$checksum = $br.ReadUInt32()
$fs.Close()
Write-Host ("CheckSum: 0x{0:X8}" -f $checksum)
Write-Host ("FileSize: {0}" -f (Get-Item $Path).Length)
